#pragma once
// Read-only reconstruction of the sole backend owner's chronological events.
// A verified release terminalizes contacts; it never counts as an explicit Up.
struct ContactLedger {
 std::map<std::uint64_t,J> active;
 std::set<std::uint64_t> down_intents;
 std::set<int> contacts;
 Ns prior_return=0;
 std::size_t released_contacts=0;
 void begin(const J& c,Ns start,Ns ret) {
  const auto intent=c.at("intent").template get<std::uint64_t>();
  const int contact=c.at("contact"),phase=c.at("phase");
  w::require(contact>=0&&contact<5&&phase>=0&&phase<=2&&start>=prior_return&&ret>=start,"sole owner receipt order/contact");
  if(phase==0){
   w::require(!down_intents.contains(intent)&&!contacts.contains(contact)&&!active.contains(intent)&&active.size()<5,"Down is unique and cannot steal active contact");
   down_intents.insert(intent);contacts.insert(contact);active.emplace(intent,c);
  }else w::require(active.contains(intent)&&active.at(intent).at("contact")==contact,"Move/Up requires own verified Down");
  prior_return=ret;
 }
 void end(const J& c){
  const auto intent=c.at("intent").template get<std::uint64_t>();const int phase=c.at("phase");
  if(phase==1)active.at(intent)=c;
  if(phase==2){contacts.erase(c.at("contact").template get<int>());active.erase(intent);}
 }
 void release(const J& e){
  w::require(e.at("dispatched").template get<bool>()&&!e.at("effect_verified").template get<bool>()&&e.at("failed_ids").is_array()&&e.at("unknown_ids").is_array()&&e.at("failed_ids").empty()&&e.at("unknown_ids").empty(),"verified backend release only; game effect remains unknown");
  w::require(e.at("start_ns").is_number_integer()&&e.at("return_ns").is_number_integer(),"release QPC integer domain");
  const Ns start=e.at("start_ns"),ret=e.at("return_ns");
  w::require(start>=prior_return&&ret>=start,"sole owner release QPC order");
  const auto& ids=e.at("requested_ids");w::require(ids.is_array()&&ids.size()<=5,"release contact bound");
  std::set<int> unique;std::vector<std::uint64_t> intents;
  for(const auto& id:ids){
   w::require(id.is_number_integer()&&id>=0&&id<5,"release contact ID");const int contact=id;
   w::require(unique.insert(contact).second&&contacts.contains(contact),"unique release of an active own contact");
   bool found=false;for(const auto& [intent,c]:active)if(c.at("contact")==contact){w::require(!found,"release own contact mapping");found=true;intents.push_back(intent);}
   w::require(found,"release requires a prior verified Down");
  }
  for(const auto intent:intents){contacts.erase(active.at(intent).at("contact").template get<int>());active.erase(intent);}
  released_contacts+=intents.size();prior_return=ret;
 }
 void finish()const{w::require(active.empty()&&contacts.empty(),"every own contact needs a verified terminal Up or chronological backend release");}
};
