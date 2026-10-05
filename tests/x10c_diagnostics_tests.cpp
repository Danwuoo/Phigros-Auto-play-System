#include "x10c_diagnostics.hpp"
#include <gtest/gtest.h>
using namespace pas;
TEST(X10cDiagnostics, OffDoesNotRecordOrRetain){x10c::drain();x10c::enabled=false;x10c::emit({{"a",1}});EXPECT_TRUE(x10c::drain().empty());}
TEST(X10cDiagnostics, DrainPreservesRecordsAndClearsBoundedBuffer){x10c::enabled=true;x10c::emit({{"a",1}});EXPECT_EQ(x10c::drain().size(),1);EXPECT_TRUE(x10c::drain().empty());x10c::enabled=false;}
TEST(X10cDiagnostics, OverflowFailsBeforeExtraRecord){x10c::enabled=true;for(int i=0;i<256;++i)x10c::emit({{"n",i}});EXPECT_THROW(x10c::emit({{"n",257}}),std::runtime_error);EXPECT_EQ(x10c::drain().size(),256);x10c::enabled=false;}
TEST(X10cDiagnostics, HistoryCannotExceedSixMeasuredPoints){GameTrackHistory t;for(int i=0;i<7;++i)t.points.push_back({});EXPECT_THROW(x10c::history_json(t),std::runtime_error);}
TEST(X10cDiagnostics, TrackCapacityDoesNotSilentlyTruncate){std::vector<GameTrackHistory> t(129);EXPECT_THROW(x10c::histories(t),std::runtime_error);}
namespace {x10c::json manifest(){return {{"schema",1},{"windows",{{{"id","K"},{"first",2865},{"last",2895},{"anchor",2883}}}}};}}
TEST(X10cWindows, EightWindowsAcceptedNinthRejected){
    auto m=manifest();for(int i=1;i<8;++i)m["windows"].push_back({{"id",std::to_string(i)},{"first",0},{"last",119},{"anchor",50}});
    EXPECT_NO_THROW(x10c::validate_windows(m));
    m["windows"].push_back({{"id","ninth"}});
    EXPECT_THROW(x10c::validate_windows(m),std::runtime_error);
}
TEST(X10cWindows, NegativeFloatAndOutOfIndexOrdinalRejected){for(const auto& v:std::vector<x10c::json>{-1,2.5,36000}){auto m=manifest();m["windows"][0]["first"]=v;EXPECT_THROW(x10c::validate_windows(m),std::runtime_error);}}
TEST(X10cWindows, AnchorOutsideRangeAndLongWindowRejected){
    auto m=manifest();m["windows"][0]["anchor"]=2896;
    EXPECT_THROW(x10c::validate_windows(m),std::runtime_error);
    m=manifest();m["windows"][0]["last"]=2985;
    EXPECT_THROW(x10c::validate_windows(m),std::runtime_error);
}
TEST(X10cWindows, DuplicateAndEmptyIdsRejected){
    auto m=manifest();m["windows"].push_back(m["windows"][0]);
    EXPECT_THROW(x10c::validate_windows(m),std::runtime_error);
    m=manifest();m["windows"][0]["id"]="";
    EXPECT_THROW(x10c::validate_windows(m),std::runtime_error);
}
