#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string_view>
namespace pas::windows_cost_comparison {
inline double floor_ms(std::string_view metric) {
 if(metric=="observer_ms")return .5;
 if(metric=="owner_ms")return .25;
 if(metric=="capture_to_owner_ms")return 3.;
 throw std::runtime_error("unsupported frozen comparison metric");
}
inline void finite_values(const std::array<double,4>& values) {
 for(const auto value:values)if(!std::isfinite(value)||value<0)throw std::runtime_error("non-finite/negative cost metric");
}
struct Noise {double d{},t{};bool adequate{};};
// Literal original protocol: disjoint pairs (1,2)/(3,4), not range or standard
// deviation. The candidate values cannot participate in this calculation.
inline Noise noise(const std::array<double,4>& a,std::string_view metric) {
 finite_values(a);const auto f=floor_ms(metric);
 const auto d=std::max(std::abs(a[0]-a[1]),std::abs(a[2]-a[3]));
 if(!std::isfinite(2*d))throw std::runtime_error("noise tolerance overflow");
 return {d,std::max(f,2*d),2*d<=std::max(*std::min_element(a.begin(),a.end())*.30,f)};
}
struct Comparison {double a{},b{},b_minus_a{},t{};bool non_regression{},improved{};};
// Each argument is one load, one quantile and one fixed A/B/B/A batch. Never
// combine loads, concatenate distributions or add phase p99 values.
inline Comparison compare(const std::array<double,4>& abba,double t) {
 finite_values(abba);if(!std::isfinite(t)||t<0)throw std::runtime_error("invalid frozen tolerance");
 const auto a=abba[0]/2+abba[3]/2,b=abba[1]/2+abba[2]/2;
 return {a,b,b-a,t,b-a<=t,a-b>t};
}
}
