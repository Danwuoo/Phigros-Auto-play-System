#include "../apps/chart_experiment/numeric_hud_metadata.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace rhythm::experimental;
unsigned groups=0,checks=0;
constexpr std::size_t rgb_bytes=1280u*720u*3u;
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
NumericHudFrame frame(std::uint64_t sequence,std::int64_t received){
    NumericHudFrame result;result.sequence=sequence;result.epoch=2;result.generation=3;result.geometry_version=4;
    result.source_sequence=sequence+100;result.source_timestamp_us=987654321;
    result.received_ns=received;result.processed_ns=received+1'000'000;return result;
}
NumericHudSubmitOutcome offer(NumericHudMetadataBudget& budget,const NumericHudFrame& f,bool idle=true,bool available=true){
    return budget.observe(f,1280,720,3840,rgb_bytes,idle,available);
}
void score(std::string_view text,std::uint32_t expected){
    const auto parsed=parse_numeric_hud_score(text);
    check(parsed.status==NumericHudParseStatus::valid&&parsed.value==expected,"valid score was not exact");
}
void bad_score(std::string_view text){check(!parse_numeric_hud_score(text).value,"invalid/ambiguous score accepted");}
void combo(std::string_view text,std::uint32_t expected,std::size_t maximum=939){
    const auto parsed=parse_numeric_hud_combo(text,maximum);
    check(parsed.status==NumericHudParseStatus::valid&&parsed.value==expected,"valid combo was not exact");
}
void bad_combo(std::string_view text,std::size_t maximum=939){
    check(!parse_numeric_hud_combo(text,maximum).value,"invalid/ambiguous combo accepted");
}
}
int main(){try{
    ++groups;{
        score("0000000",0);score("0000001",1);score("0965021",965021);score("0999999",999999);score("1000000",1000000);
        score(" \t0965021\r\n",965021);
        for(const std::string_view text:{""," ","0","965021","00965021","1000001","9999999","ooooooo","OOOOOOO",
                "O965021","0965O21","+965021","-965021","0965.21","0965,21","096 5021","SCORE 0965021",
                "0965021 0965022","0965021%","0x965021","096502l"})bad_score(text);
        bad_score(std::string_view{"0965021\0",8});
        bad_score("\xEF\xBC\x90" "965021");
        check(parse_numeric_hud_score("1000001").status==NumericHudParseStatus::out_of_range,"score range error not distinguished");
        check(parse_numeric_hud_score("965021").status==NumericHudParseStatus::wrong_length,"six-digit score not rejected by width");
    }
    ++groups;{
        combo("1",1);combo("203",203);combo("939",939);combo("000203",203);
        combo("COMBO\n203",203);combo("203 COMBO",203);combo(" \tcombo\r\n203\n",203);combo("CoMbO 1",1);
        combo("200000",200000,200000);
        for(const std::string_view text:{"","COMBO","0","000000","940","COMBO203","203COMBO","COMBO 203 COMBO",
                "203 204","COMBO 203 204","COMBO 2O3","2O3","203 MAX","421 Vax","203.0","+203","-203",
                "2,03","0000203","\xC2\xA0" "203"})bad_combo(text);
        bad_combo("1",0);bad_combo("1",200001);bad_combo("820",819);
        check(parse_numeric_hud_combo("1 2",939).status==NumericHudParseStatus::ambiguous,"multiple numeric combo tokens were not ambiguous");
        check(parse_numeric_hud_combo("1",0).status==NumericHudParseStatus::invalid_limit,"invalid maximum was not refused");
    }
    ++groups;{
        NumericHudRawText raw;raw.assign("0965021");
        check(raw.view()=="0965021"&&!raw.truncated&&!raw.non_ascii,"ASCII raw text altered");
        check(parse_numeric_hud_score(raw).value==965021,"bounded raw score parse differs");
        raw.assign(std::string(65,'0'));
        check(raw.size==64&&raw.truncated&&raw.view().size()==64,"raw text is not bounded64");
        check(parse_numeric_hud_score(raw).status==NumericHudParseStatus::raw_too_long,"truncated raw score was accepted");
        check(parse_numeric_hud_combo(raw,939).status==NumericHudParseStatus::raw_too_long,"truncated raw combo was accepted");
        raw.assign(std::string("0000000")+std::string(58,' '));
        check(!parse_numeric_hud_score(raw).value,"valid prefix concealed a clipped tail");
        raw.assign(L"0965021\u200b");
        check(raw.non_ascii&&raw.view()=="0965021?","non-ASCII raw text is not explicitly unknown");
        check(parse_numeric_hud_score(raw).status==NumericHudParseStatus::non_ascii,"Unicode ambiguity became a score");
        raw.assign(L"ooooooo");check(!parse_numeric_hud_score(raw).value,"known zero-image o confusion was silently normalized");
        raw.assign("COMBO\n203");check(parse_numeric_hud_combo(raw,939).value==203,"raw combo label handling differs");
        raw.assign(std::string(64,' '));check(raw.size==64&&!raw.truncated,"exact64 raw boundary clipped");
        check(parse_numeric_hud_score(raw).status==NumericHudParseStatus::empty,"empty64 OCR was a number");
        raw.assign("123");check(!raw.truncated&&!raw.non_ascii&&raw.view()=="123","raw reuse retained previous error flags");
    }
    ++groups;{
        NumericHudMetadataBudget budget;const auto a=frame(1,1'000'000'000);
        check(offer(budget,a)==NumericHudSubmitOutcome::submitted,"valid provenance refused");
        check(a.sequence==1&&a.source_sequence==101&&a.source_timestamp_us==987654321&&a.received_ns==1'000'000'000&&
            a.processed_ns==1'001'000'000&&a.epoch==2&&a.generation==3&&a.geometry_version==4,"provenance was renewed/rewritten");
        check(offer(budget,frame(2,1'050'000'000))==NumericHudSubmitOutcome::interval_skip,"sub100ms capture accepted");
        check(offer(budget,frame(2,1'100'000'000))==NumericHudSubmitOutcome::sequence_reject,"same sequence accepted after a skip");
        check(offer(budget,frame(3,1'100'000'000))==NumericHudSubmitOutcome::submitted,"exact100ms boundary refused");
        auto tick=a;tick.sequence=4;tick.processed_ns=2'000'000'000;
        check(offer(budget,tick)==NumericHudSubmitOutcome::timestamp_reject,"planner tick/current processing time renewed old receipt");
        check(budget.stats().submitted==2&&budget.stats().interval_skips==1&&budget.stats().sequence_rejects==1&&
            budget.stats().timestamp_rejects==1,"cadence/identity stats differ");
    }
    ++groups;{
        NumericHudMetadataBudget budget;
        check(offer(budget,frame(1,0),false)==NumericHudSubmitOutcome::busy,"busy slot was admitted");
        check(offer(budget,frame(2,50'000'000))==NumericHudSubmitOutcome::interval_skip,"busy drop permitted rapid retry");
        check(offer(budget,frame(3,100'000'000),true,false)==NumericHudSubmitOutcome::unavailable,"unavailable OCR accepted work");
        check(offer(budget,frame(4,150'000'000))==NumericHudSubmitOutcome::interval_skip,"unavailable drop bypassed cadence");
        check(offer(budget,frame(5,200'000'000))==NumericHudSubmitOutcome::submitted,"idle latest eligible capture refused");
        check(budget.stats().busy_drops==1&&budget.stats().unavailable_drops==1&&budget.stats().submitted==1,"bounded slot stats differ");
    }
    ++groups;{
        std::vector<NumericHudFrame> invalid;
        auto f=frame(1,0);f.sequence=0;invalid.push_back(f);
        f=frame(1,0);f.epoch=0;invalid.push_back(f);f=frame(1,0);f.generation=0;invalid.push_back(f);
        f=frame(1,0);f.geometry_version=0;invalid.push_back(f);f=frame(1,0);f.received_ns=-1;invalid.push_back(f);
        f=frame(1,0);f.processed_ns=-1;invalid.push_back(f);f=frame(1,0);f.source_timestamp_us=-1;invalid.push_back(f);
        for(const auto& bad:invalid){NumericHudMetadataBudget b;
            check(offer(b,bad)==NumericHudSubmitOutcome::invalid_metadata,"invalid source metadata accepted");
            check(offer(b,frame(1,0))==NumericHudSubmitOutcome::submitted,"invalid source consumed a valid identity/cadence");}
        NumericHudMetadataBudget b;f=frame(1,0);f.source_sequence.reset();f.source_timestamp_us.reset();
        check(offer(b,f)==NumericHudSubmitOutcome::submitted,"absent optional raw source metadata refused");
    }
    ++groups;{
        for(const auto geometry:std::vector<std::array<std::size_t,4>>{{720,1280,3840,rgb_bytes},{1280,721,3840,rgb_bytes},
                {1280,720,3839,rgb_bytes},{1280,720,4097,rgb_bytes},{1280,720,3840,rgb_bytes-1},
                {1280,720,3840,std::numeric_limits<std::size_t>::max()}}){
            NumericHudMetadataBudget b;
            check(b.observe(frame(1,0),static_cast<std::uint32_t>(geometry[0]),static_cast<std::uint32_t>(geometry[1]),
                static_cast<std::uint32_t>(geometry[2]),geometry[3],true,true)==NumericHudSubmitOutcome::invalid_geometry,
                "bad geometry/short/oversized RGB accepted");
            check(offer(b,frame(1,0))==NumericHudSubmitOutcome::submitted,"bad geometry consumed identity/cadence");
        }
        NumericHudMetadataBudget padded;
        check(padded.observe(frame(1,0),1280,720,4096,4096u*720u,true,true)==NumericHudSubmitOutcome::submitted,
            "bounded padded stride refused");
    }
    ++groups;{
        NumericHudMetadataBudget b(2);
        check(offer(b,frame(1,0))==NumericHudSubmitOutcome::submitted,"small budget first sample failed");
        check(offer(b,frame(2,100'000'000),false)==NumericHudSubmitOutcome::busy,"busy sample consumed row budget");
        check(offer(b,frame(3,200'000'000))==NumericHudSubmitOutcome::submitted,"small budget second sample failed");
        check(offer(b,frame(4,300'000'000))==NumericHudSubmitOutcome::capacity,"capacity exceeded");
        check(b.stats().submitted==2&&b.stats().capacity_drops==1,"capacity counter changed source count");
        NumericHudMetadataBudget empty(0);check(offer(empty,frame(1,0))==NumericHudSubmitOutcome::capacity,"zero capacity accepted work");
    }
    ++groups;{
        NumericHudMetadataBudget b(numeric_hud_max_rows+1);
        for(std::size_t i=0;i<numeric_hud_max_rows;++i)
            check(offer(b,frame(i+1,static_cast<std::int64_t>(i)*numeric_hud_interval_ns))==NumericHudSubmitOutcome::submitted,
                "legal bounded row was lost");
        check(offer(b,frame(numeric_hud_max_rows+1,static_cast<std::int64_t>(numeric_hud_max_rows)*numeric_hud_interval_ns))==
            NumericHudSubmitOutcome::capacity,"oversized configured capacity escaped7200 hard limit");
        check(b.stats().submitted==7200,"hard bounded sample count differs");
    }
    ++groups;{
        NumericHudMetadataBudget b;
        auto f=frame(1,std::numeric_limits<std::int64_t>::max()-200'000'000);
        check(offer(b,f)==NumericHudSubmitOutcome::submitted,"large valid host timestamp refused");
        f.sequence=2;f.received_ns+=numeric_hud_interval_ns;f.processed_ns=f.received_ns;
        check(offer(b,f)==NumericHudSubmitOutcome::submitted,"bounded timestamp difference overflowed");
        NumericHudRow row;row.frame=f;row.score.raw.assign("ooooooo");row.score.parse_status=NumericHudParseStatus::non_numeric;
        row.score.ocr_status=NumericHudOcrStatus::recognized;
        check(!row.score.value&&row.score.raw.view()=="ooooooo"&&row.frame.received_ns==f.received_ns,
            "unknown OCR row default invented a score or receipt");
    }
    std::cout<<groups<<" groups, "<<checks<<" checks passed (pure metadata; no Windows/OCR/data/device)\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"numeric HUD metadata failure: "<<error.what()<<'\n';return 1;}}
