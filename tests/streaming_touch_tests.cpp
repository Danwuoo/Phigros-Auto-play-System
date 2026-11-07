#include "../apps/chart_experiment/streaming_touch.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
namespace pb=android::emulation::control;
using rhythm::experimental::StreamingTouch;
using rhythm::experimental::StreamingTouchDispatch;
using rhythm::experimental::ClockProbe;
unsigned checks=0,groups=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class E,class F>void rejects(F f,const char* why){bool hit=false;try{f();}catch(const E&){hit=true;}check(hit,why);}
struct Observed {int id=0,x=0,y=0,pressure=0;};
struct Service final:pb::EmulatorController::Service {
    std::mutex mutex;std::vector<Observed> events,unary_events;std::vector<std::size_t> message_sizes;std::set<int> contacts;
    std::atomic<unsigned> streams{0},empty_streams{0},unary{0};
    std::atomic<bool> fail_finish{false};std::atomic<int> delay_finish_ms{0};
    Service(){events.reserve(10000);unary_events.reserve(10);message_sizes.reserve(10000);}
    void apply(const pb::Touch& t){if(t.pressure())contacts.insert(t.identifier());else contacts.erase(t.identifier());}
    grpc::Status streamInputEvent(grpc::ServerContext*,grpc::ServerReader<pb::InputEvent>* reader,google::protobuf::Empty*)override {
        ++streams;pb::InputEvent event;std::size_t count=0;
        while(reader->Read(&event)){
            if(!event.has_touch_event()||event.touch_event().touches_size()<1||event.touch_event().touches_size()>10)
                return {grpc::StatusCode::INVALID_ARGUMENT,"mock expects1..10 original touches"};
            std::lock_guard lock(mutex);std::set<int> message_ids;
            for(const auto& t:event.touch_event().touches()){
                if((t.pressure()!=0&&t.pressure()!=1024)||!message_ids.insert(t.identifier()).second||
                   (event.touch_event().touches_size()>1&&t.pressure()!=1024))
                    return {grpc::StatusCode::INVALID_ARGUMENT,"wrong raw amplitude, batched IDs repeat or release was batched"};
                apply(t);events.push_back({t.identifier(),t.x(),t.y(),t.pressure()});}
            message_sizes.push_back(static_cast<std::size_t>(event.touch_event().touches_size()));++count;
        }
        if(!count){++empty_streams;return grpc::Status::OK;}
        const auto delay=delay_finish_ms.exchange(0);if(delay)std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if(fail_finish.exchange(false))return {grpc::StatusCode::ABORTED,"synthetic Finish failure"};
        return grpc::Status::OK;
    }
    grpc::Status sendTouch(grpc::ServerContext*,const pb::TouchEvent* event,google::protobuf::Empty*)override {
        ++unary;std::lock_guard lock(mutex);for(const auto& t:event->touches()){
            if(t.pressure()!=0)return {grpc::StatusCode::INVALID_ARGUMENT,"mock unary path must release only"};apply(t);
            unary_events.push_back({t.identifier(),t.x(),t.y(),t.pressure()});
        }
        return grpc::Status::OK;
    }
    bool empty(){std::lock_guard lock(mutex);return contacts.empty();}
};
struct Mock {
    Service service;std::unique_ptr<grpc::Server> server;pas::GrpcEndpoint endpoint;
    Mock(){grpc::ServerBuilder builder;int port=0;builder.AddListeningPort("127.0.0.1:0",grpc::InsecureServerCredentials(),&port);
        builder.RegisterService(&service);server=builder.BuildAndStart();check(server&&port>0,"local ephemeral mock failed");
        endpoint={"127.0.0.1:"+std::to_string(port),"synthetic-local-only-token","in-process-mock"};}
    ~Mock(){if(server){server->Shutdown(std::chrono::system_clock::now()+1s);server->Wait();}}
};
struct ScriptClock final:pas::Clock {
    mutable pas::Nanoseconds value=0;mutable std::size_t at=0;std::vector<pas::Nanoseconds> readings;
    pas::Nanoseconds now_ns()const override{if(at<readings.size())value=readings[at++];return value;}
    void set(std::initializer_list<pas::Nanoseconds> next){readings=next;at=0;}
};
pas::TouchCommand command(std::size_t i,std::size_t count,pas::Nanoseconds due){
    return {1,0,i==0?pas::Phase::down:i+1==count?pas::Phase::up:pas::Phase::move,
        static_cast<double>(100+i%1000),300,due,1};
}
pas::PixelCoordinateMap mapping(){return {1280,720,1280,720,0};}
void clean_sessions(const StreamingTouch& touch){
    std::size_t index=0;for(const auto& s:touch.sessions()){
        check(s.id>=1&&s.first_event_index==index&&s.writes_done_ok&&s.finish_called&&s.finish_ok&&s.status_code==0&&!s.cancelled,
            "normal session lacks actual successful Finish evidence");
        check(s.event_count<=256,"session exceeded256 original messages");
        for(std::size_t n=0;n<s.event_count;++n){const auto& e=touch.events()[index+n];
            check(e.stream_id==s.id&&e.ordinal==n+1&&e.receipt_accepted&&e.message_write_ok&&e.host_start_ns<=e.host_return_ns,
                "ordered event/session indexing differs");}
        index+=s.event_count;
    }
    check(index==touch.events().size(),"session ledger dropped an original write");
    index=0;std::vector<std::size_t> message_counts(touch.sessions().size(),0);
    for(std::size_t m=0;m<touch.messages().size();++m){const auto& message=touch.messages()[m];
        check(message.id==m+1&&message.first_event_index==index&&message.event_count>=1&&message.event_count<=10&&
            message.write_ok&&message.host_write_start_ns<=message.host_write_return_ns,"actual message partition/timing incomplete");
        std::set<int> ids;const auto due=touch.events()[index].command.scheduled_ns;
        for(std::size_t n=0;n<message.event_count;++n){const auto& source=touch.events()[index+n];
            check(source.message_id==message.id&&source.touch_index==n+1&&source.message_write_ok&&source.stream_id==message.stream_id&&
                source.host_start_ns<=source.host_return_ns&&ids.insert(source.command.contact_id).second&&
                (message.event_count==1||(source.command.phase==pas::Phase::move&&source.command.scheduled_ns==due)),
                "source message partition crosses phase, due, ID or changes source timestamps");}
        ++message_counts.at(static_cast<std::size_t>(message.stream_id-1));index+=message.event_count;
    }
    check(index==touch.events().size(),"messages did not cover every original command");
    for(std::size_t n=0;n<message_counts.size();++n)check(touch.sessions()[n].message_count==message_counts[n],"session message count differs");
}
}
int main(){try{
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,2,5);
        touch.set_dispatch_schedule({{0,pas::Phase::down},{0,pas::Phase::down},{10'000'000,pas::Phase::move},
            {10'000'000,pas::Phase::move},{20'000'000,pas::Phase::up}});
        check(touch.prepare_connection(1000ms),"raw pressure policy mock preparation failed");
        for(int id=0;id<2;++id)check(touch.inject({static_cast<std::uint64_t>(id+1),id,pas::Phase::down,
            static_cast<double>(100+id*200),300,0,1}).success,"full-amplitude DOWN rejected");
        clock.set(10'000'000);
        for(int id=0;id<2;++id)check(touch.inject({static_cast<std::uint64_t>(id+1),id,pas::Phase::move,
            static_cast<double>(110+id*200),300,10'000'000,1}).success,"full-amplitude batched MOVE rejected");
        clock.set(20'000'000);check(touch.inject({1,0,pas::Phase::up,110,300,20'000'000,1}).success,"zero-pressure explicit UP rejected");
        const auto released=touch.release_all();
        check(mock.service.events.size()==5&&mock.service.message_sizes==std::vector<std::size_t>({1,1,2,1})&&mock.service.empty()&&
            released.failed_ids.empty()&&released.unknown_ids.empty(),"amplitude test dropped a touch or failed to release the remaining pointer");
        for(std::size_t n=0;n<5;++n)check(mock.service.events[n].pressure==(n<4?1024:0)&&touch.events()[n].pressure==(n<4?1024:0),
            "protobuf/default ledger amplitude is not raw1024 for DOWN/MOVE and0 for UP");
        check(mock.service.unary_events.size()==10,"emergency cleanup did not cover every configured release slot");
        for(const auto& event:mock.service.unary_events)check(event.pressure==0,"recovery release changed from rawzero");
        clean_sessions(touch);
    }
    ++groups;{
        Mock mock;pas::HostClock clock;constexpr std::size_t moves=4000,count=moves+4;
        StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,2,count);std::vector<StreamingTouchDispatch> schedule;
        for(std::size_t i=0;i<count;++i)schedule.push_back({i<2?0:i<count-2?1'000'000+static_cast<pas::Nanoseconds>((i-2)/2)*237'303:
            1'000'000+static_cast<pas::Nanoseconds>(moves/2)*237'303,i<2?pas::Phase::down:i<count-2?pas::Phase::move:pas::Phase::up});
        touch.set_dispatch_schedule(schedule);check(touch.prepare_connection(1000ms),"dense batch preparation failed");
        unsigned queue_guards=0,message_guards=0;touch.before_write=[&](const pas::TouchCommand&){++queue_guards;return std::string{};};
        touch.before_message_write=[&](const pas::TouchCommand&){++message_guards;return std::string{};};
        const auto origin=clock.now_ns()+10'000'000;std::vector<pas::Nanoseconds> original_returns;original_returns.reserve(count);
        for(std::size_t i=0;i<count;++i){const auto due=origin+schedule[i].song_ns;while(clock.now_ns()<due)std::this_thread::yield();const int id=static_cast<int>(i%2);
            const auto receipt=touch.inject({static_cast<std::uint64_t>(700+id),id,schedule[i].phase,static_cast<double>(100+i%1000),300,due,1});
            check(receipt.success&&receipt.reason=="command_queue_accepted_effect_unverified","dense queue admission failed or claimed Write/ACK");original_returns.push_back(receipt.injection_return_ns);
            if(i==2)check(touch.messages().size()==2&&touch.events()[2].message_id==0&&!touch.events()[2].message_write_ok,"first MOVE was sent before the equal-due group existed");
        }
        const auto released=touch.release_all();check(released.unknown_ids.empty()&&mock.service.events.size()==count&&touch.messages().size()==moves/2+4&&
            queue_guards==count&&message_guards==count&&mock.service.empty(),"dense batching omitted original Touches or failed to halve MOVE messages");
        for(std::size_t i=0;i<count;++i){const auto& actual=mock.service.events[i];check(actual.id==static_cast<int>(i%2)&&actual.x==100+static_cast<int>(i%1000)&&actual.y==300&&
            actual.pressure==(i<count-2?1024:0)&&touch.events()[i].host_return_ns==original_returns[i],"original order/payload changed or queued receipt was retimed");}
        std::cout<<"4000 original MOVEs at two each237303ns ->2000 InputEvents; original Touches intact.\n";clean_sessions(touch);
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;pas::PixelCoordinateMap rotated(1280,720,720,1280,90);StreamingTouch touch(clock,mock.endpoint,rotated,720,1280,10,30);
        std::vector<StreamingTouchDispatch> schedule;for(int n=0;n<30;++n)schedule.push_back({n<10?0:n<20?50'000'000:100'000'000,
            n<10?pas::Phase::down:n<20?pas::Phase::move:pas::Phase::up});touch.set_dispatch_schedule(schedule);
        check(touch.prepare_connection(1000ms),"ten-finger batch preparation failed");
        for(int n=0;n<30;++n){const int id=n%10;clock.set(schedule[static_cast<std::size_t>(n)].song_ns);
            check(touch.inject({static_cast<std::uint64_t>(id+1),id,schedule[static_cast<std::size_t>(n)].phase,static_cast<double>(100+id*100+(n>=10?5:0)),
                static_cast<double>(100+id*40+(n>=10?7:0)),clock.now_ns(),1}).success,"ten-finger source command omitted");}
        const auto released=touch.release_all();check(released.unknown_ids.empty()&&touch.messages().size()==21&&mock.service.events.size()==30&&
            mock.service.message_sizes[10]==10&&mock.service.empty(),"ten MOVE IDs were not retained in one physical TouchEvent");
        for(int n=0;n<30;++n){const int id=n%10,x=100+id*100+(n>=10?5:0),y=100+id*40+(n>=10?7:0);const auto& got=mock.service.events[static_cast<std::size_t>(n)];
            check(got.id==id&&got.x==719-y&&got.y==x&&got.pressure==(n<20?1024:0),"ten-finger batch rotation90 mapping differs");}clean_sessions(touch);
    }
    ++groups;{
        for(int mode=0;mode<3;++mode){
            Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,2,4);
            touch.set_dispatch_schedule({{0,pas::Phase::down},{0,pas::Phase::down},{1'000'000,pas::Phase::move},{1'000'000,pas::Phase::move}});
            check(touch.prepare_connection(1000ms),"local-queue cancellation setup failed");
            for(int id=0;id<2;++id)check(touch.inject({static_cast<std::uint64_t>(id+1),id,pas::Phase::down,100,300,0,1}).success,"pending setup DOWN failed");
            clock.set(1'000'000);const auto first=touch.inject({1,0,pas::Phase::move,101,300,1'000'000,1});check(first.success,"first local MOVE not accepted");
            if(mode==1){clock.set(21'000'001);check(!touch.poll().empty(),"local queue survived20ms");}
            if(mode==2){touch.before_message_write=[](const pas::TouchCommand& c){return c.contact_id==0?std::string("synthetic_actual_UI_denied_member0"):std::string{};};
                check(!touch.inject({2,1,pas::Phase::move,201,300,1'000'000,1}).success,"queued member was not guarded before actual Write");}
            // mode0 is an outer STOP with no callback/inject for the final MOVE.
            const auto released=touch.release_all();check(touch.messages().size()==2&&touch.events()[2].receipt_accepted&&touch.events()[2].message_id==0&&touch.events()[2].touch_index==0&&
                !touch.events()[2].message_write_ok&&touch.events()[2].host_return_ns==first.injection_return_ns&&touch.sessions().back().cancelled&&!touch.sessions().back().finish_ok&&
                released.unknown_ids.size()==10&&mock.service.unary==10&&mock.service.empty(),"discarded local commands were written, retimed or falsely acknowledged");
            for(const auto& got:mock.service.events)check(got.x==100,"STOP sent a pending local MOVE");
        }
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,5);
        touch.set_dispatch_schedule({{0,pas::Phase::down},{1'000'000,pas::Phase::move},{1'000'000,pas::Phase::move},{1'000'000,pas::Phase::move},{2'000'000,pas::Phase::up}});
        check(touch.prepare_connection(1000ms),"duplicate-ID splitting setup failed");
        for(std::size_t n=0;n<5;++n){const auto phase=n==0?pas::Phase::down:n==4?pas::Phase::up:pas::Phase::move;clock.set(n==0?0:n==4?2'000'000:1'000'000);
            check(touch.inject({1,0,phase,static_cast<double>(100+n),300,clock.now_ns(),1}).success,"same-ID update was discarded");}
        check(touch.release_all().unknown_ids.empty()&&touch.messages().size()==5&&mock.service.events.size()==5,"same-ID updates were merged or dropped");clean_sessions(touch);
        Mock invalid_mock;pas::FakeClock invalid_clock;StreamingTouch invalid(invalid_clock,invalid_mock.endpoint,mapping(),1280,720,1,3);
        rejects<std::invalid_argument>([&]{invalid.set_dispatch_schedule({{0,pas::Phase::down}});},"partial dispatch accepted");
        rejects<std::invalid_argument>([&]{invalid.set_dispatch_schedule({{0,pas::Phase::down},{2,pas::Phase::move},{1,pas::Phase::up}});},"unsorted dispatch accepted");
        rejects<std::invalid_argument>([&]{invalid.set_dispatch_schedule({{0,pas::Phase::down},{-1,pas::Phase::move},{2,pas::Phase::up}});},"negative dispatch accepted");
        rejects<std::invalid_argument>([&]{invalid.set_dispatch_schedule({{0,pas::Phase::down},{1,static_cast<pas::Phase>(99)},{2,pas::Phase::up}});},"unknown dispatch phase accepted");
        invalid.set_dispatch_schedule({{0,pas::Phase::down},{1,pas::Phase::move},{2,pas::Phase::up}});check(invalid.prepare_connection(1000ms),"schedule mismatch setup failed");
        check(invalid.inject(command(0,3,0)).success,"schedule mismatch DOWN failed");invalid_clock.set(2);
        check(!invalid.inject(command(1,3,2)).success&&invalid.events().size()==1&&invalid.fault()=="stream_dispatch_schedule_mismatch","wrong fixed offset reached queue");invalid.release_all();
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,3);
        touch.set_dispatch_schedule({{0,pas::Phase::down},{1'000'000,pas::Phase::move},{2'000'000,pas::Phase::up}});
        check(touch.prepare_connection(1000ms)&&touch.inject(command(0,3,0)).success,"sparse MOVE setup failed");
        clock.set(1'000'000);check(touch.inject(command(1,3,1'000'000)).success&&touch.messages().size()==2&&touch.messages().back().event_count==1,
            "sparse MOVE remained pending instead of a single immediate message");
        touch.before_message_write=[](const pas::TouchCommand& c){return c.phase==pas::Phase::up?std::string("synthetic_UP_actual_write_denied"):std::string{};};
        clock.set(2'000'000);check(!touch.inject(command(2,3,2'000'000)).success&&mock.service.unary==10&&mock.service.empty()&&touch.messages().size()==2&&
            touch.events().back().message_id==0&&!touch.events().back().receipt_accepted,"UP actual-admission denial omitted immediate independent all10 release");
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,6);
        check(touch.prepare_connection(1000ms),"reusable backend preparation failed");
        clock.set(1'000'000);const auto idle=touch.release_all();
        check(idle.requested_ids.empty()&&idle.failed_ids.empty()&&idle.unknown_ids.empty()&&idle.start_ns==0&&idle.return_ns==0&&
            mock.service.streams==1&&mock.service.unary==0&&mock.service.events.empty(),
            "Scheduler arm idle cancel emitted input or invented release timestamps");
        unsigned guards=0;touch.before_write=[&](const pas::TouchCommand&){++guards;return std::string{};};
        for(std::size_t i=0;i<3;++i){clock.set(static_cast<pas::Nanoseconds>(i+1)*1'000'000);
            check(touch.inject(command(i,3,clock.now_ns())).success,"idle arm cancel revoked the prepared backend");}
        clock.set(4'000'000);const auto actual=touch.release_all();
        check(actual.requested_ids.size()==10&&actual.failed_ids.empty()&&actual.unknown_ids.empty()&&actual.start_ns==4'000'000&&
            actual.return_ns==4'000'000&&mock.service.unary==10&&mock.service.events.size()==3&&mock.service.empty(),
            "first actual cleanup did not Finish and perform exactly one real all10 release");
        const auto sessions=touch.sessions().size();clock.set(5'000'000);const auto cached=touch.release_all();
        check(cached.requested_ids==actual.requested_ids&&cached.failed_ids==actual.failed_ids&&cached.unknown_ids==actual.unknown_ids&&
            cached.start_ns==actual.start_ns&&cached.return_ns==actual.return_ns&&mock.service.unary==10&&touch.sessions().size()==sessions,
            "idle cleanup replayed unary releases or replaced cached real report times");
        for(std::size_t i=0;i<3;++i){clock.set(static_cast<pas::Nanoseconds>(i+6)*1'000'000);auto next=command(i,3,clock.now_ns());next.intent_id=2;
            check(touch.inject(next).success,"healthy completed backend could not be reused without preparation");}
        clock.set(9'000'000);const auto second=touch.release_all();
        check(second.requested_ids.size()==10&&second.start_ns==9'000'000&&second.return_ns==9'000'000&&mock.service.unary==20&&
            mock.service.events.size()==6&&guards==12&&touch.fault().empty()&&mock.service.empty(),
            "reused backend lost source writes, guards or its distinct real cleanup");clean_sessions(touch);
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;pas::PixelCoordinateMap rotated(1280,720,720,1280,90);
        StreamingTouch touch(clock,mock.endpoint,rotated,720,1280,10,30);
        check(touch.prepare_connection(1000ms),"ten-contact rotated preparation failed");
        for(int id=0;id<10;++id)check(touch.inject({static_cast<std::uint64_t>(id+1),id,pas::Phase::down,
            static_cast<double>(100+id*100),static_cast<double>(100+id*40),0,1}).success,"ten-contact DOWN omitted");
        clock.set(50'000'000);check(touch.poll().empty(),"ten-contact first rollover failed");
        {std::lock_guard lock(mock.service.mutex);check(mock.service.contacts.size()==10,"rollover lost active physical slots");}
        for(int id=9;id>=0;--id)check(touch.inject({static_cast<std::uint64_t>(id+1),id,pas::Phase::move,
            static_cast<double>(105+id*100),static_cast<double>(107+id*40),50'000'000,2}).success,"interleaved ten-contact MOVE omitted");
        clock.set(100'000'000);check(touch.poll().empty(),"ten-contact second rollover failed");
        {std::lock_guard lock(mock.service.mutex);check(mock.service.contacts.size()==10,"second stream reset active physical IDs");}
        for(int id=0;id<10;++id)check(touch.inject({static_cast<std::uint64_t>(id+1),id,pas::Phase::up,
            static_cast<double>(105+id*100),static_cast<double>(107+id*40),100'000'000,3}).success,"ten-contact UP omitted");
        const auto release=touch.release_all();check(release.unknown_ids.empty()&&mock.service.events.size()==30&&mock.service.empty(),"ten-contact cleanup incomplete");
        for(std::size_t i=0;i<30;++i){const int id=i<10?static_cast<int>(i):i<20?19-static_cast<int>(i):static_cast<int>(i)-20;
            const int x=(i<10?100:105)+id*100,y=(i<10?100:107)+id*40;const auto& got=mock.service.events[i];
            check(got.id==id&&got.x==719-y&&got.y==x&&got.pressure==(i<20?1024:0)&&touch.events()[i].command.intent_id==static_cast<std::uint64_t>(id+1),
                "rotation90 physical mapping, original intent, ID, FIFO or release pressure changed");}
        clean_sessions(touch);
    }
    ++groups;{
        Mock mock;ScriptClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,4);
        check(touch.prepare_connection(1000ms)&&touch.inject(command(0,4,0)).success,"boundary setup failed");
        clock.set({49'999'999,50'000'001,50'000'002});
        check(touch.inject(command(1,4,50'000'000)).success&&touch.fault().empty(),"existing stream crossing50ms between checks was spuriously faulted");
        touch.release_all();
        Mock slow_mock;ScriptClock slow_clock;StreamingTouch slow(slow_clock,slow_mock.endpoint,mapping(),1280,720,1,4);
        check(slow.prepare_connection(1000ms),"slow-open setup failed");slow_clock.set({0,50'000'000,50'000'001});
        check(!slow.inject(command(0,4,0)).success&&slow.events().empty()&&slow.fault()=="stream_open_time_budget_exceeded",
            "slow new stream consumed its deadline margin and still emitted input");
    }
    ++groups;{
        Mock mock;pas::HostClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,2,10);
        check(touch.prepare_connection(1000ms),"zero-event stream probe failed");
        check(mock.service.streams==1&&mock.service.empty_streams==1&&mock.service.events.empty()&&mock.service.unary==0&&mock.service.empty(),
            "preparation emitted touch data or an unexpected RPC");
        const auto& p=touch.preparation();check(p.connected&&p.probe_opened&&p.probe_writes_done_ok&&p.probe_finish_ok&&p.probe_status_code==0&&
            p.input_rpc_opened==1&&p.touch_events==0&&p.begin_ns<=p.return_ns,"preparation trace falsely claims zero RPC or touches");
        for(const auto invalid:{-1ms,0ms,2001ms})rejects<std::invalid_argument>([&]{touch.prepare_connection(invalid);},"unbounded preparation timeout accepted");
        clean_sessions(touch);
    }
    ++groups;{
        Mock mock;pas::HostClock clock;constexpr std::size_t count=5000;
        StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,2,count);check(touch.prepare_connection(1000ms),"rate mock preparation failed");
        const auto origin=clock.now_ns()+10'000'000;const auto before=std::chrono::steady_clock::now();
        for(std::size_t i=0;i<count;++i){const auto due=origin+static_cast<pas::Nanoseconds>(i*1'000'000'000ULL/4500);
            while(clock.now_ns()<due)std::this_thread::yield();
            const auto receipt=touch.inject(command(i,count,due));
            if(!receipt.success||receipt.reason!="command_queue_accepted_effect_unverified")
                throw std::runtime_error("4500Hz source write "+std::to_string(i)+" rejected: "+receipt.reason+
                    "; start_minus_due_ns="+std::to_string(receipt.injection_start_ns-due)+
                    "; call_duration_ns="+std::to_string(receipt.injection_return_ns-receipt.injection_start_ns));
            check(true,"4500Hz source write accepted with explicit queue semantics");
        }
        const auto release=touch.release_all();check(release.failed_ids.empty()&&release.unknown_ids.empty()&&release.requested_ids.size()==10,
            "normal final release did not cover all10 slots");
        check(touch.fault().empty()&&mock.service.events.size()==count&&touch.events().size()==count&&mock.service.empty(),"rate path omitted or replayed original writes");
        for(std::size_t i=0;i<count;++i){const auto& got=mock.service.events[i];check(got.id==0&&got.x==100+static_cast<int>(i%1000)&&got.y==300&&
            got.pressure==(i+1==count?0:1024),"mock FIFO order/coordinates/pressure changed");}
        check(touch.sessions().size()>15&&mock.service.unary==10,"bounded rollovers or distinct unary release missing");clean_sessions(touch);
        std::cout<<"5000 FIFO writes at4500Hz scheduled cadence completed in "
            <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-before).count()<<"ms; Write is provisional.\n";
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,600);
        check(touch.prepare_connection(1000ms),"rollover preparation failed");
        for(std::size_t i=0;i<257;++i)check(touch.inject(command(i,600,0)).success,"constant-clock rollover lost a write");
        check(touch.sessions().size()==3&&touch.sessions()[1].event_count==256&&touch.sessions()[1].finish_ok,
            "256-event rollover did not finish before a new stream");
        clock.set(50'000'000);check(touch.poll().empty()&&touch.sessions().back().finish_ok,"50ms idle poll failed to Finish the live stream");
        const auto release=touch.release_all();check(release.unknown_ids.empty()&&mock.service.events.size()==257&&mock.service.empty(),"idle rollover omitted writes or release");clean_sessions(touch);
    }
    ++groups;{
        Mock mock;pas::HostClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,4);
        check(touch.prepare_connection(1000ms),"deadline preparation failed");check(touch.inject(command(0,4,clock.now_ns())).success,"deadline DOWN setup failed");
        std::this_thread::sleep_for(125ms);check(!touch.poll().empty(),"100ms context deadline was silently ignored");
        check(touch.sessions().back().finish_called&&!touch.sessions().back().finish_ok,"expired stream falsely claimed Finish OK");
        const auto release=touch.release_all();check(release.requested_ids.size()==10&&release.unknown_ids.size()==10&&mock.service.unary==10&&mock.service.empty(),
            "deadline uncertainty was erased or not released across all10 slots");
        check(!touch.fault().empty(),"unary cleanup erased streaming uncertainty");
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,300);
        check(touch.prepare_connection(1000ms),"Finish failure preparation failed");mock.service.fail_finish=true;
        for(std::size_t i=0;i<256;++i)check(touch.inject(command(i,300,0)).success,"pre-Finish write was not provisionally accepted");
        const auto receipt=touch.inject(command(256,300,0));
        check(!receipt.success&&touch.events().size()==256&&!touch.fault().empty()&&!touch.sessions().back().finish_ok,
            "failed Finish was treated as successful source transmission or replayed");
        const auto release=touch.release_all();check(release.unknown_ids.size()==10&&mock.service.unary==10&&mock.service.empty(),"Finish failure did not retain uncertainty and release10");
    }
    ++groups;{
        Mock mock;pas::HostClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,300);
        check(touch.prepare_connection(1000ms),"guard rollover preparation failed");
        for(std::size_t i=0;i<256;++i)check(touch.inject(command(i,300,clock.now_ns())).success,"guard rollover setup failed");
        mock.service.delay_finish_ms=30;const auto due=clock.now_ns();unsigned calls=0;
        touch.before_write=[&](const pas::TouchCommand&){++calls;return clock.now_ns()-due>20'000'000?
            std::string("mock_actual_UI_expired_after_rollover"):std::string{};};
        const auto receipt=touch.inject(command(256,300,due));
        check(!receipt.success&&calls==1&&touch.events().size()==256&&touch.fault()=="mock_actual_UI_expired_after_rollover",
            "before_write was not rechecked after blocking rollover Finish");touch.release_all();
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,4);
        check(touch.prepare_connection(1000ms),"late planned UP preparation failed");check(touch.inject(command(0,4,0)).success,"UP setup DOWN failed");
        clock.set(20'000'001);auto up=command(3,4,0);const auto receipt=touch.inject(up);
        check(!receipt.success&&touch.events().size()==1&&mock.service.unary==10&&mock.service.empty(),
            "late planned UP counted as a successful source step instead of independent safety release");
        check(touch.unary_release().requested_ids.size()==10,"actual unary emergency report missing");
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,4);
        check(touch.prepare_connection(1000ms),"guard exception preparation failed");
        touch.before_write=[](const pas::TouchCommand&)->std::string{throw std::runtime_error("synthetic");};
        check(!touch.inject(command(0,4,0)).success&&touch.events().empty()&&touch.fault()=="stream_before_write_guard_exception",
            "guard exception allowed an unguarded input write");touch.release_all();
        rejects<std::invalid_argument>([&]{StreamingTouch remote(clock,{"example.com:1234","synthetic","mock"},mapping(),1280,720,1,4);},"remote streaming endpoint accepted");
        rejects<std::invalid_argument>([&]{StreamingTouch invalid(clock,mock.endpoint,mapping(),1280,720,1,65537);},"unbounded trace budget accepted");
    }
    ++groups;{
        Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,4);
        const std::vector<StreamingTouchDispatch> schedule={{0,pas::Phase::down},{32'000'000,pas::Phase::up},
            {400'000'000,pas::Phase::down},{432'000'000,pas::Phase::up}};
        const ClockProbe probe{true,200'000'000,160'000'000};touch.set_dispatch_schedule(schedule,probe);
        check(touch.prepare_connection(1000ms),"clock probe mock preparation failed");
        for(std::size_t i=0;i<schedule.size();++i) {
            const auto due=1'000'000'000+probe.mapped_song_ns(schedule[i].song_ns);clock.set(due);
            const pas::TouchCommand cmd{1,0,schedule[i].phase,100,300,due,1};
            check(touch.inject(cmd).success,"stream rejected the exact static suffix offset");
        }
        check(touch.events().size()==4&&touch.events()[3].command.scheduled_ns-touch.events()[2].command.scheduled_ns==32'000'000,
            "stream clock probe changed contact internal dwell");
        touch.release_all();check(mock.service.empty()&&touch.fault().empty(),"clock probe mock did not complete and release");
    }
    ++groups;{
        for(const bool omit_offset:{false,true}) {
            Mock mock;pas::FakeClock clock;StreamingTouch touch(clock,mock.endpoint,mapping(),1280,720,1,4);
            const std::vector<StreamingTouchDispatch> schedule={{0,pas::Phase::down},{32'000'000,pas::Phase::up},
                {400'000'000,pas::Phase::down},{432'000'000,pas::Phase::up}};
            const ClockProbe probe{true,200'000'000,160'000'000};touch.set_dispatch_schedule(schedule,probe);
            check(touch.prepare_connection(1000ms),"clock probe rejection mock preparation failed");
            for(std::size_t i=0;i<2;++i) {
                const auto due=1'000'000'000+schedule[i].song_ns;clock.set(due);
                check(touch.inject({1,0,schedule[i].phase,100,300,due,1}).success,"clock probe rejection prefix failed");
            }
            const auto correct=1'000'000'000+probe.mapped_song_ns(schedule[2].song_ns);
            const auto scheduled=omit_offset?1'000'000'000+schedule[2].song_ns:correct;
            clock.set(omit_offset?scheduled:correct+20'000'001);
            check(!touch.inject({2,0,pas::Phase::down,100,300,scheduled,1}).success&&touch.events().size()==2&&
                touch.fault()==(omit_offset?"stream_dispatch_schedule_mismatch":"stream_compiled_step_lateness_limit_exceeded"),
                "clock probe relaxed fixed-base schedule or20ms actual-write guard");
            touch.release_all();
        }
    }
    std::cout<<groups<<" groups / "<<checks<<" streaming touch checks passed; in-process mock only, no emulator or device input.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
