#include "pas/manual_score_budget.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    using pas::ManualScoreBudget;
    constexpr pas::Nanoseconds second=1'000'000'000LL,start=100*second;
    unsigned checks=0;
    const auto check=[&](bool pass){++checks;if(!pass)throw std::runtime_error("score session boundary failed");};
    const auto rejects=[&](pas::Nanoseconds watchdog,unsigned rounds){
        try {ManualScoreBudget budget(start,watchdog,rounds);return false;}
        catch(const std::invalid_argument&){return true;}
    };
    check(rejects(600*second,0));check(rejects(600*second,21));
    check(rejects(0,20));check(rejects(601*second,20));
    ManualScoreBudget single(start,600*second,1);
    check(!single.waiting_expired(start+300*second-1));check(single.waiting_expired(start+300*second));
    check(!single.wall_expired(start+900*second-1));check(single.wall_expired(start+900*second));
    check(single.finish(true,start+200*second)&&single.completed()==1);
    ManualScoreBudget twenty(start,600*second,20);
    for(unsigned round=1;round<=20;++round) {
        const auto now=start+round*240*second;
        check(twenty.finish(true,now)==(round==20));
        check(twenty.completed()==round);
        // A completed song resets the standby timer even after hours elapsed.
        check(!twenty.waiting_expired(now+1'800*second-1));
        check(twenty.waiting_expired(now+1'800*second));
    }
    check(!twenty.wall_expired(start+18'000*second-1));check(twenty.wall_expired(start+18'000*second));
    ManualScoreBudget aborted(start,600*second,20);
    check(aborted.finish(false,start+100*second)&&aborted.completed()==0);
    std::cout<<checks<<" score-session boundary checks passed; no device or gameplay files.\n";
}
