#pragma once
#include "pas/emulator.hpp"
#include "clock_probe.hpp"
#include <grpc/impl/channel_arg_names.h>
#include <grpc/support/time.h>
#include <array>
#include <chrono>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rhythm::experimental {
struct StreamingTouchDispatch {pas::Nanoseconds song_ns=0;pas::Phase phase=pas::Phase::down;};
struct StreamingTouchEvent {
    pas::TouchCommand command{};
    pas::Nanoseconds host_start_ns=0,host_return_ns=0;
    std::uint64_t stream_id=0;
    std::size_t ordinal=0;
    int mapped_x=0,mapped_y=0,pressure=0;
    bool receipt_accepted=false,message_write_ok=false;
    std::uint64_t message_id=0;
    std::size_t touch_index=0;
};
struct StreamingTouchMessage {
    std::uint64_t id=0,stream_id=0;
    std::size_t first_event_index=0,event_count=0;
    pas::Nanoseconds host_write_start_ns=0,host_write_return_ns=0;
    bool write_ok=false;
};
struct StreamingTouchSession {
    std::uint64_t id=0;
    bool preparation_probe=false;
    pas::Nanoseconds open_begin_ns=0,open_return_ns=0,close_begin_ns=0,close_return_ns=0;
    std::size_t first_event_index=0,event_count=0,message_count=0;
    bool writes_done_ok=false,finish_called=false,finish_ok=false,cancelled=false;
    int status_code=-1;
};
struct StreamingTouchPreparation {
    pas::Nanoseconds begin_ns=0,return_ns=0;
    int timeout_ms=0;
    bool connected=false,probe_opened=false,probe_writes_done_ok=false,probe_finish_ok=false;
    int probe_status_code=-1;
    std::size_t input_rpc_opened=0,touch_events=0;
};

// Touch.pressure is the raw ABS_MT_PRESSURE integer, not MotionEvent's
// normalized float. The project's validated emulator axis is0..1024 and
// standard local UI presses use1024. Version025 selects that full amplitude;
// this does not establish any proprietary game's pressure acceptance rule.
struct StreamingTouchPressurePolicy {
    static constexpr const char* id="sdk-raw-axis-full-scale-1024-up-zero-v1";
    static constexpr int active_pressure=1024,up_pressure=0;
};

// Source receipts accept local queue entries. Equal-due MOVEs may share one
// TouchEvent, preserving every original Touch and its order. Write/Finish do
// not prove Android input effects. Never retry or replay unknown messages.
class StreamingTouch final:public pas::TouchBackend {
    using Pb=android::emulation::control::InputEvent;
    static constexpr pas::Nanoseconds rotate_ns=50'000'000,late_ns=20'000'000,pending_context_ns=75'000'000;
    static constexpr std::size_t rotate_events=256,max_steps=65536,none=std::numeric_limits<std::size_t>::max();
    const pas::Clock& clock_;
    pas::GrpcEndpoint endpoint_;
    pas::PixelCoordinateMap mapping_;
    int width_,height_,capacity_;
    std::size_t expected_steps_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<android::emulation::control::EmulatorController::Stub> stub_;
    pas::GrpcTouch recovery_;
    std::unique_ptr<grpc::ClientContext> context_;
    std::unique_ptr<grpc::ClientWriter<Pb>> writer_;
    google::protobuf::Empty response_;
    Pb event_;
    std::vector<StreamingTouchEvent> events_;
    std::vector<StreamingTouchMessage> messages_;
    std::vector<StreamingTouchSession> sessions_;
    std::vector<StreamingTouchDispatch> dispatch_;
    ClockProbe clock_probe_;
    std::size_t current_=none,pending_count_=0;
    std::array<std::size_t,10> pending_{};
    pas::Nanoseconds dispatch_offset_ns_=0,pending_due_ns_=0;
    bool dispatch_bound_=false;
    std::array<bool,10> active_{},unknown_{};
    StreamingTouchPreparation preparation_;
    pas::ReleaseReport unary_release_;
    std::string fault_;
    bool prepared_=false,stream_uncertain_=false,touch_attempted_=false;
    static pas::GrpcEndpoint local_endpoint(pas::GrpcEndpoint endpoint) {
        const auto colon=endpoint.target.rfind(':');
        if(colon==std::string::npos||endpoint.token.empty())throw std::invalid_argument("invalid local streaming endpoint");
        const auto host=endpoint.target.substr(0,colon),port=endpoint.target.substr(colon+1);
        if((host!="127.0.0.1"&&host!="localhost")||port.empty()||port.size()>5)throw std::invalid_argument("streaming input endpoint must be local loopback");
        unsigned value=0;for(const unsigned char c:port){if(c<'0'||c>'9')throw std::invalid_argument("invalid local streaming port");value=value*10+c-'0';}
        if(value<1||value>65535)throw std::invalid_argument("invalid local streaming port");return endpoint;
    }
    void fail(const std::string& reason,bool uncertain=false) {
        if(fault_.empty())fault_=reason;
        if(uncertain){stream_uncertain_=true;unknown_.fill(true);}
    }
    gpr_timespec deadline()const {return gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC),gpr_time_from_millis(timeout_.count(),GPR_TIMESPAN));}
    bool open_stream(bool probe,gpr_timespec outer=gpr_inf_future(GPR_CLOCK_MONOTONIC)) {
        if(writer_||!fault_.empty())return false;
        if(sessions_.size()>=expected_steps_+1){fail("stream_session_trace_budget");return false;}
        StreamingTouchSession session;session.id=sessions_.size()+1;session.preparation_probe=probe;
        session.first_event_index=events_.size();session.open_begin_ns=clock_.now_ns();sessions_.push_back(session);current_=sessions_.size()-1;
        context_=std::make_unique<grpc::ClientContext>();context_->AddMetadata("authorization","Bearer "+endpoint_.token);
        auto until=deadline();if(gpr_time_cmp(outer,until)<0)until=outer;context_->set_deadline(until);
        response_.Clear();writer_=stub_->streamInputEvent(context_.get(),&response_);sessions_[current_].open_return_ns=clock_.now_ns();
        if(!writer_){sessions_[current_].status_code=static_cast<int>(grpc::StatusCode::UNAVAILABLE);context_.reset();current_=none;fail("stream_open_failed",true);return false;}return true;
    }
    void close_stream(bool cancel) {
        if(!writer_)return;
        auto& session=sessions_[current_];session.close_begin_ns=clock_.now_ns();
        if(pending_count_){
            // Pending commands are local only. STOP discards them instead of
            // turning cleanup into a new message containing queued MOVEs.
            cancel=true;fail("stream_pending_commands_discarded",true);pending_count_=0;
        }
        session.cancelled=cancel;if(cancel)context_->TryCancel();
        session.writes_done_ok=writer_->WritesDone();session.finish_called=true;
        const auto status=writer_->Finish();session.status_code=static_cast<int>(status.error_code());session.finish_ok=status.ok();
        session.close_return_ns=clock_.now_ns();writer_.reset();context_.reset();current_=none;
        if(!session.writes_done_ok||!session.finish_ok)fail("stream_finish_not_confirmed",true);
    }
    bool guard(const pas::TouchCommand& command,bool actual_message=false) {
        const auto& callback=actual_message&&before_message_write?before_message_write:before_write;
        if(callback){std::string reason;try{reason=callback(command);}catch(...){reason="stream_before_write_guard_exception";}
            if(!reason.empty()){fail(reason);return false;}}
        return true;
    }
    bool due(const pas::TouchCommand& command,pas::Nanoseconds start) {
        if(command.scheduled_ns<0||start<command.scheduled_ns||start-command.scheduled_ns>late_ns){fail("stream_compiled_step_lateness_limit_exceeded");return false;}return true;
    }
    bool flush_pending() {
        if(!pending_count_)return fault_.empty();
        // Every queued original gets a fresh guard, then all share the actual
        // message write start for the unchanged20ms due check.
        for(std::size_t n=0;n<pending_count_;++n)if(!guard(events_[pending_[n]].command,true)){close_stream(true);return false;}
        event_.mutable_touch_event()->clear_touches();
        for(std::size_t n=0;n<pending_count_;++n){const auto& source=events_[pending_[n]];auto* touch=event_.mutable_touch_event()->add_touches();
            touch->set_identifier(source.command.contact_id);touch->set_x(source.mapped_x);touch->set_y(source.mapped_y);touch->set_pressure(source.pressure);}
        const auto start=clock_.now_ns();
        for(std::size_t n=0;n<pending_count_;++n)if(!due(events_[pending_[n]].command,start)){close_stream(true);return false;}
        if(pending_count_>1&&(start<sessions_[current_].open_begin_ns||start-sessions_[current_].open_begin_ns>=pending_context_ns)){
            fail("stream_pending_commands_time_limit_exceeded",true);close_stream(true);return false;
        }
        const auto message_id=messages_.size()+1;messages_.push_back({message_id,sessions_[current_].id,pending_[0],pending_count_,start,0,false});
        auto& message=messages_.back();touch_attempted_=true;
        for(std::size_t n=0;n<pending_count_;++n){auto& source=events_[pending_[n]];source.message_id=message_id;source.touch_index=n+1;}
        message.write_ok=writer_->Write(event_);message.host_write_return_ns=clock_.now_ns();++sessions_[current_].message_count;
        for(std::size_t n=0;n<pending_count_;++n){auto& source=events_[pending_[n]];source.message_write_ok=message.write_ok;
            const auto id=static_cast<std::size_t>(source.command.contact_id);if(message.write_ok){active_[id]=source.command.phase!=pas::Phase::up;unknown_[id]=false;}}
        pending_count_=0;if(!message.write_ok){fail("stream_write_result_unknown",true);close_stream(true);return false;}return fault_.empty();
    }
    pas::TouchReceipt rejected(const pas::TouchCommand& command) {const auto current=clock_.now_ns();return {command,current,current,false,fault_};}
public:
    std::function<std::string(const pas::TouchCommand&)> before_write;
    std::function<std::string(const pas::TouchCommand&)> before_message_write;
    StreamingTouch(const pas::Clock& clock,pas::GrpcEndpoint endpoint,pas::PixelCoordinateMap mapping,int width,int height,int capacity,std::size_t expected_steps,
            std::chrono::milliseconds timeout=std::chrono::milliseconds(100)):
        clock_(clock),endpoint_(local_endpoint(std::move(endpoint))),mapping_(mapping),width_(width),height_(height),capacity_(capacity),expected_steps_(expected_steps),timeout_(timeout),
        recovery_(clock,endpoint_,mapping,width,height,10,timeout) {
        if(width<2||height<2||width>65535||height>65535||capacity<1||capacity>10||expected_steps<1||expected_steps>max_steps||timeout<=std::chrono::milliseconds(0)||timeout>std::chrono::milliseconds(100))
            throw std::invalid_argument("invalid bounded streaming configuration");
        events_.reserve(expected_steps);messages_.reserve(expected_steps);sessions_.reserve(expected_steps+1);
        events_.resize(expected_steps);events_.clear();messages_.resize(expected_steps);messages_.clear();sessions_.resize(expected_steps+1);sessions_.clear();
        grpc::ChannelArguments args;args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL,1);channel_=grpc::CreateCustomChannel(endpoint_.target,grpc::InsecureChannelCredentials(),args);
        stub_=android::emulation::control::EmulatorController::NewStub(channel_);
        auto* touches=event_.mutable_touch_event()->mutable_touches();touches->Reserve(10);
        for(int n=0;n<10;++n)event_.mutable_touch_event()->add_touches();event_.mutable_touch_event()->clear_touches();
    }
    ~StreamingTouch()override {try{if(touch_attempted_)release_all();else if(writer_)close_stream(true);}catch(...){}}
    StreamingTouch(const StreamingTouch&)=delete;
    StreamingTouch& operator=(const StreamingTouch&)=delete;
    void set_dispatch_schedule(std::vector<StreamingTouchDispatch> schedule,ClockProbe probe={}) {
        if(prepared_||!sessions_.empty()||!events_.empty()||writer_||!fault_.empty())throw std::logic_error("dispatch schedule requires unused unprepared backend");
        if(schedule.size()!=expected_steps_)throw std::invalid_argument("dispatch schedule size differs from expected original steps");
        validate_clock_probe_options(probe);
        pas::Nanoseconds previous=0,previous_mapped=0;bool first=true;for(const auto& step:schedule){
            if(step.song_ns<0||step.song_ns<previous||(step.phase!=pas::Phase::down&&step.phase!=pas::Phase::move&&step.phase!=pas::Phase::up))throw std::invalid_argument("invalid immutable dispatch schedule");
            const auto mapped=probe.mapped_song_ns(step.song_ns);
            if(!first&&((step.song_ns==previous&&mapped!=previous_mapped)||(step.song_ns>previous&&mapped<=previous_mapped)))
                throw std::invalid_argument("clock probe changes immutable dispatch order or ties");
            previous=step.song_ns;previous_mapped=mapped;first=false;
        }dispatch_=std::move(schedule);clock_probe_=probe;
    }
    bool prepare_connection(std::chrono::milliseconds timeout) {
        if(timeout<=std::chrono::milliseconds(0)||timeout>std::chrono::seconds(2))throw std::invalid_argument("invalid streaming preparation timeout");
        if(!events_.empty()||writer_||!fault_.empty())throw std::logic_error("stream preparation requires unused healthy backend");if(prepared_)return channel_->GetState(false)==GRPC_CHANNEL_READY;
        preparation_.begin_ns=clock_.now_ns();preparation_.timeout_ms=static_cast<int>(timeout.count());
        const auto until=gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC),gpr_time_from_millis(timeout.count(),GPR_TIMESPAN));
        preparation_.connected=channel_->WaitForConnected(until)&&channel_->GetState(false)==GRPC_CHANNEL_READY;
        if(preparation_.connected&&open_stream(true,until)){
            preparation_.probe_opened=true;preparation_.input_rpc_opened=1;close_stream(false);const auto& probe=sessions_.back();
            preparation_.probe_writes_done_ok=probe.writes_done_ok;preparation_.probe_finish_ok=probe.finish_ok;preparation_.probe_status_code=probe.status_code;prepared_=probe.writes_done_ok&&probe.finish_ok;
        }if(!prepared_)fail("stream_preparation_failed");preparation_.return_ns=clock_.now_ns();return prepared_;
    }
    std::string poll() {
        if(writer_){const auto& session=sessions_[current_];const auto current=clock_.now_ns();
            if(current<session.open_begin_ns){fail("stream_clock_regressed",true);close_stream(true);}
            else if(pending_count_){const auto first=events_[pending_[0]].host_start_ns;
                if(current<first||!fault_.empty()||current-first>late_ns||current-session.open_begin_ns>=pending_context_ns){fail("stream_pending_commands_time_limit_exceeded",true);close_stream(true);}
            }else if(session.event_count>=rotate_events||current-session.open_begin_ns>=rotate_ns)close_stream(!fault_.empty());
        }return fault_;
    }
    pas::TouchReceipt inject(const pas::TouchCommand& command)override {
        const bool up=command.phase==pas::Phase::up;
        if(command.contact_id<0||command.contact_id>=capacity_||(command.phase!=pas::Phase::down&&command.phase!=pas::Phase::move&&!up))throw std::invalid_argument("invalid streaming contact/phase");
        const auto point=mapping_.map(command.x,command.y);
        if(point[0]<0||point[1]<0||point[0]>=width_||point[1]>=height_)throw std::invalid_argument("streaming touch outside display");
        const auto id=static_cast<std::size_t>(command.contact_id);
        if(!prepared_||!fault_.empty()){if(fault_.empty())fail("stream_not_prepared");if(up)release_all();return rejected(command);}
        if((command.phase==pas::Phase::down&&active_[id])||(command.phase!=pas::Phase::down&&!active_[id]))throw std::invalid_argument("invalid streaming contact lifecycle");
        if(events_.size()>=expected_steps_){fail("stream_event_trace_budget");if(up)release_all();return rejected(command);}
        if(!dispatch_.empty()){
            const auto& expected=dispatch_[events_.size()];if(command.scheduled_ns<0||expected.phase!=command.phase){fail("stream_dispatch_schedule_mismatch");if(up)release_all();return rejected(command);}
            const auto offset=command.scheduled_ns-clock_probe_.mapped_song_ns(expected.song_ns);
            if(dispatch_bound_&&offset!=dispatch_offset_ns_){fail("stream_dispatch_schedule_mismatch");if(up)release_all();return rejected(command);}
            if(!dispatch_bound_){dispatch_offset_ns_=offset;dispatch_bound_=true;}
        }
        if(pending_count_&&(command.phase!=pas::Phase::move||command.scheduled_ns!=pending_due_ns_)){fail("stream_pending_dispatch_mismatch");if(up)release_all();return rejected(command);}
        poll();if(!fault_.empty()){if(up)release_all();return rejected(command);}
        // Repeated IDs remain separate ordered messages; never combine two
        // updates of one physical slot into a purported simultaneous frame.
        for(std::size_t n=0;n<pending_count_;++n)if(events_[pending_[n]].command.contact_id==command.contact_id){if(!flush_pending())return rejected(command);break;}
        if(!writer_){if(!open_stream(false))return rejected(command);
            if(clock_.now_ns()-sessions_[current_].open_begin_ns>=rotate_ns){fail("stream_open_time_budget_exceeded");close_stream(true);if(up)release_all();return rejected(command);}}
        if(!guard(command)){if(up)release_all();return rejected(command);}
        const auto start=clock_.now_ns();if(!due(command,start)){if(up)release_all();return rejected(command);}
        auto& session=sessions_[current_];const auto index=events_.size();
        events_.push_back({command,start,0,session.id,session.event_count+1,point[0],point[1],
            up?StreamingTouchPressurePolicy::up_pressure:StreamingTouchPressurePolicy::active_pressure,true,false,0,0});++session.event_count;
        pending_[pending_count_++]=index;pending_due_ns_=command.scheduled_ns;
        const bool following_same_move=!dispatch_.empty()&&command.phase==pas::Phase::move&&index+1<dispatch_.size()&&dispatch_[index+1].phase==pas::Phase::move&&dispatch_[index+1].song_ns==dispatch_[index].song_ns;
        bool accepted=true;if(!following_same_move||pending_count_==10||session.event_count==rotate_events)accepted=flush_pending();
        if(!accepted&&up)release_all();
        auto& source=events_[index];source.receipt_accepted=accepted;source.host_return_ns=clock_.now_ns();
        return {command,start,source.host_return_ns,accepted,accepted?"command_queue_accepted_effect_unverified":fault_};
    }
    pas::ReleaseReport release_all()override {
        bool any_active=false;for(const auto active:active_)any_active=any_active||active;
        if(!touch_attempted_&&!any_active&&!writer_&&fault_.empty())return unary_release_;
        if(writer_)close_stream(!fault_.empty()||pending_count_!=0);
        unary_release_=recovery_.emergency_release_all();auto report=unary_release_;
        if(stream_uncertain_){report.unknown_ids.clear();for(int id=0;id<10;++id)report.unknown_ids.push_back(id);}
        if(!report.failed_ids.empty()||!report.unknown_ids.empty())fail("stream_release_unconfirmed");else{active_.fill(false);unknown_.fill(false);touch_attempted_=false;}return report;
    }
    const std::vector<StreamingTouchEvent>& events()const noexcept{return events_;}
    const std::vector<StreamingTouchMessage>& messages()const noexcept{return messages_;}
    const std::vector<StreamingTouchSession>& sessions()const noexcept{return sessions_;}
    const StreamingTouchPreparation& preparation()const noexcept{return preparation_;}
    const pas::ReleaseReport& unary_release()const noexcept{return unary_release_;}
    const std::string& fault()const noexcept{return fault_;}
};
} // namespace rhythm::experimental
