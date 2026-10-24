#include "../fixtures/android/evidence/pointer_ledger.hpp"
#include <iostream>
#include <stdexcept>
#include <initializer_list>

namespace {
int checks=0;
void require(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);++checks;}
std::array<bool,32> present(std::initializer_list<std::size_t> ids) {
    std::array<bool,32> result{};for(auto id:ids)result.at(id)=true;return result;
}
}
int main() {
    using namespace pas_fixture_evidence;
    try {
        for(std::size_t capacity:{2,5,10,32}) {
            PointerLedger ledger;auto seen=present({0});ledger.observe(PointerAction::down,0,seen);
            for(std::size_t i=1;i<capacity;++i) {seen[i]=true;ledger.observe(PointerAction::pointer_down,i,seen);}
            require(ledger.count()==capacity&&ledger.errors==0,"valid multi-pointer down");
            ledger.observe(PointerAction::move,0,seen);
            for(std::size_t i=capacity-1;i>0;--i) {ledger.observe(PointerAction::pointer_up,i,seen);seen[i]=false;}
            ledger.observe(PointerAction::up,0,seen);
            require(ledger.count()==0&&ledger.errors==0&&ledger.saw_pointer,"valid multi-pointer release");
        }
        {
            PointerLedger ledger;
            ledger.observe(PointerAction::down,0,present({0}));
            ledger.observe(PointerAction::pointer_down,1,present({1,2}));
            ledger.observe(PointerAction::pointer_up,0,present({0,1}));
            ledger.observe(PointerAction::up,1,present({1}));
            require(ledger.count()==0&&ledger.errors>0,"review regression: inconsistent full snapshot cannot look clean");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,0,present({0,1}));
            require(ledger.errors>0,"DOWN must carry only changed pointer");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::pointer_down,1,present({1}));
            require(ledger.errors>0,"POINTER_DOWN requires an existing stream");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,0,present({0}));
            ledger.observe(PointerAction::pointer_down,0,present({0}));
            require(ledger.errors>0,"duplicate pointer down rejected");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,0,present({0}));
            ledger.observe(PointerAction::pointer_down,1,present({0,1}));
            ledger.observe(PointerAction::pointer_up,1,present({1}));
            require(ledger.errors>0,"POINTER_UP requires complete pre-release snapshot");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,0,present({0}));
            ledger.observe(PointerAction::pointer_up,0,present({0}));
            require(ledger.errors>0,"last pointer requires UP rather than POINTER_UP");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,0,present({0}));
            ledger.observe(PointerAction::pointer_down,1,present({0,1}));
            ledger.observe(PointerAction::up,1,present({0,1}));
            require(ledger.errors>0,"UP must close entire stream");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,31,present({31}));
            ledger.observe(PointerAction::move,0,present({0}));
            require(ledger.errors>0,"MOVE snapshot detects lost or invented pointer");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,31,present({31}));
            ledger.observe(PointerAction::cancel,0,present({31}));
            require(ledger.errors==0&&ledger.count()==0,"Android CANCEL clears observed active pointers");
            ledger.observe(PointerAction::up,31,present({31}));
            require(ledger.errors>0,"UP after CANCEL is orphan");
        }
        {
            PointerLedger ledger;ledger.observe(PointerAction::down,32,present({0}));
            require(ledger.errors>0&&ledger.count()==0,"out of bound pointer never indexes array");
        }
        std::cout<<checks<<" pointer ledger checks passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
