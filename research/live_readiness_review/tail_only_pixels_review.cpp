// Independent current-pixel controls under the frozen tail-only review plan.
// The raster implementation is the unchanged existing synthetic test raster.
#define main preserved_current_contract_main
#include "../prelive_current/current_tests.cpp"
#undef main
#include "../prelive_current/tail_current.hpp"
void establish(Own&o){for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);o.step(a);}auto body=make(4,70*ms,550,200,410);o.step(body);}
bool terminal_only(const pas::current_rails::Evaluation&e){return e.valid&&e.filtered.targets.size()==1&&e.filtered.targets[0].current_contact==GameTarget::CurrentContact::hold_tail_only&&e.filtered.targets[0].tail_terminal_proof.has_value();}
void set_rgb(Frame&f,int x0,int y0,int x1,int y1,std::array<int,3>rgb){for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)if(x>=0&&x<f.width&&y>=0&&y<f.height)for(int c=0;c<3;++c)f.rgb[(std::size_t(y)*f.width+x)*3+c]=std::uint8_t(rgb[c]);}
int main(int argc,char**argv){Tests r;try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh independent pixels report required");
 {Own o;establish(o);const auto move=o.backend.moves;auto a=make(5,90*ms,550,512,410);auto e=o.step(a);r.check("pixel_terminal_without_body_permission",terminal_only(e)&&!e.support[0].body&&o.backend.downs==1&&o.backend.moves==move);auto b=make(6,110*ms,550,512,410);e=o.step(b);o.clock.set(130*ms);o.owner.poll();o.sync();r.check("pixel_terminal_finishes_only_own_contact",terminal_only(e)&&o.backend.downs==1&&o.backend.moves==move&&o.backend.ups==1);r.check("pixel_positive_release",o.finish());}
 const std::vector<std::string> names={"no_own_down","source_invalid","batch_frame_mismatch","scene_line_mismatch","no_current_line","association_invalid","projected_line","competing_line","duplicate_candidate","duplicate_target","overlapping_candidate","missing_right_rail","no_cyan_fill","yellow_effect","white_only_fill","tail_unknown","body_patch","tail_roi_clipped","left_roi_clipped","multiple_terminals","expired_frame","context_changed","tail_neighbour","wrong_line_id","stale_line"};
 for(std::size_t which=0;which<names.size();++which){Own o;if(which!=0)establish(o);auto a=make(5,90*ms,550,512,410);auto both=[&](auto change){change(a.b.candidates[0].note);change(a.s.targets[0].note);};
  if(which==1)a.f.source_valid=false;
  if(which==2)++a.b.context.frame;
  if(which==3)a.s.lines[0].center.y+=1;
  if(which==4){a.b.lines.clear();a.s.lines.clear();}
  if(which==5){a.b.lines[0].association_valid=false;a.s.lines=a.b.lines;}
  if(which==6)a.s.targets[0].line_projection_only=true;
  if(which==7){auto l=a.b.lines[0];l.track_id=8;a.b.lines.push_back(l);a.s.lines=a.b.lines;}
  if(which==8)a.b.candidates.push_back(a.b.candidates[0]);
  if(which==9){auto t=a.s.targets[0];t.note_id=43;a.s.targets.push_back(t);}
  if(which==10){auto c=a.b.candidates[0];c.candidate_id=2;a.b.candidates.push_back(c);}
  if(which==11)set_rgb(a.f,458,510,463,552,{30,30,30});
  if(which==12)set_rgb(a.f,362,512,458,551,{30,30,30});
  if(which==13)set_rgb(a.f,362,512,458,551,{230,190,70});
  if(which==14)set_rgb(a.f,362,512,458,551,{255,255,255});
  if(which==15)both([](auto&n){n.tail.reset();});
  if(which==16){both([](auto&n){n.held_body_patch=true;});a.b.candidates[0].head_visible=false;}
  if(which==17)both([](auto&n){n.tail=Vec2{410,-100};});
  if(which==18)a=make(5,90*ms,550,512,12);
  if(which==19)set_rgb(a.f,358,521,463,528,{30,30,30}); // two separated cyan/rail terminals inside the unchanged65-rowROI
  if(which==21){a.f.epoch=2;a.b.context.epoch=a.s.context.epoch=2;}
  if(which==22){auto c=a.b.candidates[0];c.candidate_id=2;c.note.kind=NoteKind::tap;c.note.center={410,512};c.note.tail.reset();c.note.width=78;a.b.candidates.push_back(c);}
  if(which==23)a.s.targets[0].line_id=8;
  if(which==24){a.b.lines[0].observed_ns-=ms;a.s.lines=a.b.lines;}
  o.clock.set(which==20?190*ms:a.f.pixels_ready_ns);const auto before_down=o.backend.downs,before_move=o.backend.moves;auto e=o.hook.evaluate(a.f,a.b,a.s,o.ledger,o.clock.now_ns());r.details[names[which]]={{"valid",e.valid},{"accepted",e.accepted},{"support_count",e.count},{"tail_only",terminal_only(e)}};r.check("pixels_deny_"+names[which],!terminal_only(e)&&o.backend.downs==before_down&&o.backend.moves==before_move);r.check("pixels_release_"+names[which],o.finish());
 }
 {Own o;establish(o);const auto move=o.backend.moves;for(int k=0;k<2;++k){auto a=make(5+k,(90+k*20)*ms,550,512,410,.03*(k+1),false,{155,233,255},.03*(k+1));const auto e=o.step(a);r.check("rotating_line_tail_current_local_coordinates_"+std::to_string(k),terminal_only(e)&&o.backend.downs==1&&o.backend.moves==move);}o.clock.set(130*ms);o.owner.poll();o.sync();r.check("rotating_tail_same_contact_up",o.backend.ups==1&&o.backend.downs==1&&o.backend.moves==move);r.check("rotating_tail_release",o.finish());}
 J out={{"schema","pas.independent-typed-tail-pixels-review.v1"},{"assertions",r.rows.size()},{"failed",r.failed},{"checks",r.rows},{"details",r.details},{"device_endpoints",0},{"physical_gold",0}};std::ofstream f(argv[1]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("write");return r.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
