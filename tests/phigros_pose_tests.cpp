#include "rhythm/phigros_pose.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;
void require(bool ok, const std::string& label) { if (!ok) throw std::runtime_error(label); ++checks; }
void near(double actual, double expected, const std::string& label, double tolerance=1e-9) {
    require(std::isfinite(actual) && std::abs(actual-expected) <= tolerance, label);
}
template<class Function> void rejects(Function function, const std::string& label) {
    bool failed = false;
    try { function(); } catch (const std::exception&) { failed = true; }
    require(failed, label);
}
rhythm::ChartEventTime time(rhythm::SongNs start, rhythm::SongNs end) {
    rhythm::ChartEventTime t; t.start_ns=start; t.end_ns=end; return t;
}
rhythm::MoveLineEvent move(rhythm::SongNs a, rhythm::SongNs b, double x, double ex, double y=.5, double ey=.5) {
    rhythm::MoveLineEvent e; e.time=time(a,b); e.start_x=x; e.end_x=ex; e.start_y=y; e.end_y=ey; return e;
}
rhythm::NumericLineEvent number(rhythm::SongNs a, rhythm::SongNs b, double x, double ex) {
    rhythm::NumericLineEvent e; e.time=time(a,b); e.start=x; e.end=ex; return e;
}
rhythm::SemanticChart chart() {
    rhythm::SemanticChart c; rhythm::ChartLine line; line.bpm=120;
    line.move_events={move(0,100,.5,.5)}; line.rotate_events={number(0,100,0,0)};
    line.opacity_events={number(0,100,1,1)}; c.lines.push_back(line); return c;
}
void run(const char* name, const std::function<void()>& test) { test(); std::cout << "PASS " << name << '\n'; }
}
int main() {
    using namespace rhythm;
    try {
        run("static native positionX scale and top-left origin", [] {
            auto c=chart(); c.lines[0].move_events={move(0,100,.5,.5,.25,.25)};
            PhigrosPoseEvaluator e(c); const auto p=e.line_at(0,50);
            near(p.origin.x,.5,"center x"); near(p.origin.y,.75,"native y inversion");
            auto n=phigros_line_point(p,2,NoteSide::above);
            near(n.reference_pixels.x,784,"positionX times72"); near(n.reference_pixels.y,540,"native y pixels");
            near(n.normalized.x,.6125,"normalized x"); require(n.inside_reference,"static inside");
        });
        run("linear movement rotation and raw opacity", [] {
            auto c=chart(); c.lines[0].move_events={move(0,100,0,1,0,1)};
            c.lines[0].rotate_events={number(0,100,0,90)}; c.lines[0].opacity_events={number(0,100,-1,2)};
            PhigrosPoseEvaluator e(c); auto p=e.line_at(0,50);
            near(p.origin.x,.5,"move midpoint x"); near(p.origin.y,.5,"move midpoint y");
            near(p.rotation_degrees,45,"rotation midpoint"); near(p.opacity,.5,"opacity raw midpoint");
            near(e.line_at(0,0).opacity,-1,"negative raw opacity retained"); near(e.line_at(0,100).opacity,2,"above-one raw opacity retained");
        });
        run("rotation signs and above below normals", [] {
            auto c=chart(); c.lines[0].rotate_events={number(0,100,90,90)};
            auto p=PhigrosPoseEvaluator(c).line_at(0,50);
            auto above=phigros_line_point(p,1,NoteSide::above), below=phigros_line_point(p,1,NoteSide::below);
            near(above.normalized.x,.5,"positive90 x"); near(above.normalized.y,.4,"positive90 screen-up tangent");
            near(above.normalized.x,below.normalized.x,"below does not mirror x"); near(above.normalized.y,below.normalized.y,"same line center");
            near(above.side_normal.x,-1,"above90 normal left"); near(below.side_normal.x,1,"below90 normal right");
            near(phigros_line_point(p,0,NoteSide::above,72).reference_pixels.x,568,"explicit above displacement");
            c.lines[0].rotate_events={number(0,100,-90,-90)};
            near(PhigrosPoseEvaluator(c).note_target(0,SemanticNote{.position_x=1},50).normalized.y,.6,"negative90 screen-down");
            c.lines[0].rotate_events={number(0,100,450,450)};
            near(PhigrosPoseEvaluator(c).note_target(0,SemanticNote{.position_x=1},50).normalized.y,.4,"full turn equivalent");
        });
        run("hold tracks current line and ignores approach speed floor", [] {
            constexpr SongNs end=60000000000; // A sixty-second hold, not an instantaneous example.
            auto c=chart(); c.lines[0].move_events={move(0,end,.5,.6)}; c.lines[0].rotate_events={number(0,end,0,90)};
            SemanticNote hold; hold.kind=NoteKind::hold; hold.position_x=1; hold.start_ns=0; hold.end_ns=end;
            hold.speed=99; hold.floor_position=1234; PhigrosPoseEvaluator e(c);
            near(e.note_target(0,hold,0).reference_pixels.x,712,"hold starts at initial line");
            const auto mid=e.note_target(0,hold,end/2);
            near(mid.normalized.x,.55+72*std::sqrt(.5)/1280,"hold noncardinal midpoint x",1e-6);
            near(mid.normalized.y,.5-72*std::sqrt(.5)/720,"hold noncardinal midpoint y",1e-6);
            near(e.note_target(0,hold,end).reference_pixels.x,768,"hold follows moving rotated line x");
            near(e.note_target(0,hold,end).reference_pixels.y,288,"hold follows rotated line y");
        });
        run("shared boundaries gaps endpoint hold and zero steps", [] {
            auto c=chart(); c.lines[0].move_events={move(0,10,.1,.3),move(10,20,.8,.9)};
            PhigrosPoseEvaluator e(c); near(e.line_at(0,10).origin.x,.8,"later interval wins boundary");
            near(e.line_at(0,-1).origin.x,.1,"before-first holds start"); near(e.line_at(0,99).origin.x,.9,"after-last holds end");
            c.lines[0].move_events={move(0,0,.2,.7),move(20,30,.8,.9)};
            PhigrosPoseEvaluator z(c); near(z.line_at(0,-1).origin.x,.2,"before instant uses start");
            near(z.line_at(0,0).origin.x,.7,"instant uses end"); near(z.line_at(0,10).origin.x,.7,"gap holds preceding end");
            c.lines[0].move_events={move(0,0,.2,.4),move(0,0,.4,.8)};
            near(PhigrosPoseEvaluator(c).line_at(0,0).origin.x,.8,"same-time zero latest source wins");
        });
        run("missing unordered overlap reversed nonfinite rejection", [] {
            auto c=chart(); c.lines[0].move_events.clear(); rejects([&]{PhigrosPoseEvaluator e(c);},"missing track refused");
            c=chart(); c.lines[0].move_events={move(10,20,.1,.2),move(0,10,.2,.3)};
            rejects([&]{PhigrosPoseEvaluator e(c);},"unordered refused");
            c.lines[0].move_events={move(0,20,.1,.2),move(10,30,.2,.3)};
            rejects([&]{PhigrosPoseEvaluator e(c);},"overlap refused");
            c.lines[0].move_events={move(20,10,.1,.2)}; rejects([&]{PhigrosPoseEvaluator e(c);},"reversed refused");
            c=chart(); c.lines[0].rotate_events[0].end=std::numeric_limits<double>::quiet_NaN();
            rejects([&]{PhigrosPoseEvaluator e(c);},"NaN event refused");
            c=chart(); SemanticNote n; n.position_x=std::numeric_limits<double>::infinity();
            rejects([&]{PhigrosPoseEvaluator(c).note_target(0,n,0);},"infinite note refused");
            SemanticChart empty; PhigrosPoseEvaluator e(empty); require(e.line_count()==0,"empty chart valid");
            rejects([&]{e.line_at(0,0);},"missing line refused");
        });
        run("indexed arbitrary queries and extreme SongNs avoid subtraction overflow", [] {
            auto c=chart(); c.lines[0].move_events.clear();
            for (int i=0;i<33000;++i) c.lines[0].move_events.push_back(move(i*10,(i+1)*10,i/33000.,(i+1)/33000.));
            PhigrosPoseEvaluator e(c); require(e.line_at(0,329995).move_event_index==32999,"last indexed event");
            require(e.line_at(0,15).move_event_index==1,"reverse seek indexed event");
            c=chart(); c.lines[0].move_events={move(std::numeric_limits<SongNs>::min(),std::numeric_limits<SongNs>::max(),0,1)};
            near(PhigrosPoseEvaluator(c).line_at(0,0).origin.x,.5,"full signed range midpoint");
        });
        run("identity geometry boundary pixels and offscreen rejection", [] {
            PhigrosGeometryMap m({1280,720}); auto p=m.map_normalized({.25,.75});
            require(p.status==GeometryStatus::mapped && p.pixel && p.pixel->x==320 && p.pixel->y==540,"identity pixels");
            p=m.map_normalized({1,1}); require(p.pixel && p.pixel->x==1279 && p.pixel->y==719,"upper boundary inward raster");
            p=m.map_normalized({0,0}); require(p.pixel && p.pixel->x==0 && p.pixel->y==0,"lower boundary pixel");
            p=m.map_normalized({1.01,.5}); require(!p.pixel && p.status==GeometryStatus::outside_reference,"offscreen no clamp");
            auto c=chart(); auto target=phigros_line_point(PhigrosPoseEvaluator(c).line_at(0,0),100,NoteSide::above);
            require(!target.inside_reference && !m.map_reference_pixels(target.reference_pixels).pixel,"offscreen note rejected without normal projection");
        });
        run("letterbox viewport and quarter-turn mapping normalized goldens", [] {
            PhigrosGeometryMap letterbox({1920,1200}); auto content=letterbox.content_viewport();
            near(content.top,60,"letterbox top"); near(content.height,1080,"letterbox height");
            auto top=letterbox.map_normalized({.5,0}), bottom=letterbox.map_normalized({.5,1});
            require(top.pixel && top.pixel->y==60,"top content pixel"); require(bottom.pixel && bottom.pixel->y==1139,"bottom content edge stays inward");
            PhigrosGeometry geometry{720,1280,InputRotation::clockwise_90}; PhigrosGeometryMap r90(geometry);
            auto p=r90.map_normalized({.25,.75}); near(p.input_normalized.x,.25,"90 x normalized",1e-6); near(p.input_normalized.y,.25,"90 y normalized",1e-6);
            require(p.pixel && p.pixel->x==180 && p.pixel->y==320,"90 pixels");
            geometry.rotation=InputRotation::clockwise_270; p=PhigrosGeometryMap(geometry).map_normalized({.25,.75});
            near(p.input_normalized.x,.75,"270 x",1e-6); near(p.input_normalized.y,.75,"270 y",1e-6);
            PhigrosGeometryMap r180({1280,720,InputRotation::clockwise_180}); p=r180.map_normalized({.25,.75});
            near(p.input_normalized.x,.75,"180 x",1e-6); near(p.input_normalized.y,.25,"180 y",1e-6);
            geometry={1600,1000}; geometry.viewport=PixelViewport{160,140,1280,720}; geometry.fit=ViewportFit::stretch;
            p=PhigrosGeometryMap(geometry).map_normalized({.5,.5}); near(p.input_normalized.x,.5,"viewport centered x",1e-6); near(p.input_normalized.y,.5,"viewport centered y",1e-6);
        });
        run("invalid geometry and NaN points rejected", [] {
            rejects([]{PhigrosGeometryMap m({0,720});},"zero width");
            PhigrosGeometry g{1280,720}; g.viewport=PixelViewport{-1,0,100,100};
            rejects([&]{PhigrosGeometryMap m(g);},"outside viewport");
            g.viewport=PixelViewport{0,0,std::numeric_limits<double>::quiet_NaN(),100};
            rejects([&]{PhigrosGeometryMap m(g);},"NaN viewport");
            PhigrosGeometryMap m({1280,720}); rejects([&]{m.map_normalized({NAN,.5});},"NaN map point");
        });
        std::cout << checks << " pose/geometry checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
