#include "contact_policy.hpp"
#include <array>
#include <cmath>

namespace bvi {
namespace {
constexpr std::uint64_t budget=6291456;
constexpr double epsilon=1e-6;
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y};}
Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}
Point mul(Point a,double k){return {a.x*k,a.y*k};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
Point tangent(double a){return {std::cos(a),std::sin(a)};}
Point normal(double a){return {-std::sin(a),std::cos(a)};}
bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool in_frame(const View&f,Point p){return finite(p)&&p.x>=0&&p.y>=0&&p.x<f.width&&p.y<f.height;}
bool in_body(const Query&q,Point p){
 const auto d=sub(p,q.front);const double s=dot(d,tangent(q.angle)),k=dot(d,normal(q.angle));
 return std::isfinite(s)&&std::isfinite(k)&&std::abs(s)<=q.width/2+epsilon&&k<=epsilon&&k>=-q.depth-epsilon;
}
bool in_segment(const Line&l,Point p){const auto s=dot(sub(p,l.center),tangent(l.angle));return std::isfinite(s)&&std::abs(s)<=l.length/2+epsilon;}
struct Pixel {
 int r{},g{},b{};bool in{};
 bool blue()const{return in&&r>=20&&r<=100&&g>=130&&g<=230&&b>=180&&b<=255&&b>=g+20;}
 bool yellow()const{return in&&r>=180&&g>=120&&b<=180;}
};
Pixel pixel(const View&f,Point p,std::uint64_t&probes){
 // The enclosing extract treats budget+1 as invalid. Saturate before overflow
 // and never read additional pixels once the global budget is exhausted.
 if(probes>=budget){probes=budget+1;return {};}
 ++probes;
 // Preserve donor llround semantics for Tap, without converting huge doubles.
 if(!finite(p)||p.x<=-.5||p.y<=-.5||p.x>=f.width-.5||p.y>=f.height-.5)return {};
 const auto x=static_cast<int>(std::llround(p.x)),y=static_cast<int>(std::llround(p.y));
 const auto i=static_cast<std::size_t>(y)*f.stride+static_cast<std::size_t>(x)*3;
 return {f.rgb[i],f.rgb[i+1],f.rgb[i+2],true};
}
bool valid(const View&f,const Query&q,const Line&l,Point hit){
 return f.key.source_valid&&f.width>0&&f.height>0&&f.width<=1280&&f.height<=720&&f.stride>=f.width*3&&
  f.rgb.size()==static_cast<std::size_t>(f.stride)*f.height&&finite(q.front)&&std::isfinite(q.width)&&
  std::isfinite(q.depth)&&std::isfinite(q.angle)&&q.width>=4&&q.width<=4096&&q.depth>0&&q.depth<=4095&&
  finite(l.center)&&std::isfinite(l.angle)&&std::isfinite(l.length)&&l.length>0&&finite(hit);
}
}
ContactSample sample_current_contact(const View&f,const Query&q,const Line&l,Point hit,std::uint64_t&probes){
 ContactSample result;
 if(!valid(f,q,l,hit))return result;
 const auto ln=normal(l.angle),tu=tangent(q.angle),nv=normal(q.angle);
 if(q.tap){
  // Deliberately retain all eight donor reads, including duplicated delta=3.
  for(int sign:{-1,1}){bool ok=true;for(int delta:std::array<int,2>{3,3}){
   const auto cp=add(hit,mul(ln,sign*delta));
   const auto a=pixel(f,add(cp,mul(tu,-.35*q.width)),probes),b=pixel(f,add(cp,mul(tu,.35*q.width)),probes);
   result.yellow|=a.yellow()||b.yellow();ok&=a.blue()&&b.blue();
  }result.supported|=ok;}
  if(probes>budget)result.supported=false;
  return result;
 }
 // A geometric hit only locates the query. It never supplies pixel evidence.
 if(!in_frame(f,hit)||!in_body(q,hit)||!in_segment(l,hit)||
    std::abs(dot(sub(hit,l.center),ln))>epsilon||
    std::abs(dot(sub(hit,q.front),tu))>epsilon)return result;
 const double den=dot(nv,ln);
 if(!std::isfinite(den)||std::abs(den)<epsilon)return result;
 std::array<Point,2>flank_hits{};
 for(std::size_t side=0;side<2;++side){
  const auto anchor=add(hit,mul(tu,(side==0?-.35:.35)*q.width));
  const double along=-dot(sub(anchor,l.center),ln)/den;
  flank_hits[side]=add(anchor,mul(nv,along));
  if(!in_frame(f,flank_hits[side])||!in_segment(l,flank_hits[side]))return result;
 }
 for(int sign:{-1,1}){
  bool ok=true;
  for(int delta:{4,6})for(const auto flank:flank_hits){
   const auto p=add(flank,mul(ln,sign*delta));
   if(!in_frame(f,p)||!in_body(q,p)||!in_segment(l,p)){ok=false;continue;}
   const Point center{double(std::llround(p.x)),double(std::llround(p.y))};
   if(!in_frame(f,center)||!in_body(q,center)||!in_segment(l,center)){ok=false;continue;}
   const auto sample=pixel(f,p,probes);result.yellow|=sample.yellow();ok&=sample.blue();
  }
  result.supported|=ok;
 }
 if(probes>budget)result.supported=false;
 return result;
}
}
