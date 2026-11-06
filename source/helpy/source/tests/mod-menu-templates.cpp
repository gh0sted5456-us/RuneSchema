#include "Core/ModMenuTemplates.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace PS::ModMenuTemplates;
template<class F>void Reject(F fn){bool failed=false;try{fn();}catch(...){failed=true;}assert(failed);}
int main(){
 Json page={{"Id","Example"},{"Version","1.2"},{"Revision",4},{"TemplateContext","character-a"},{"Helpy",{{"templates",{{"id","appearance"},{"version",1},{"keys",{"Shape","Enabled","Style","Caption"}},{"contextVariable","Example.character"}}}}},{"Settings",Json::array({{{"key","Shape"},{"type","slider"},{"min",0},{"max",100}},{{"key","Enabled"},{"type","toggle"}},{{"key","Style"},{"type","choice"},{"options",{"A","B"}}},{{"key","Caption"},{"type","text"},{"maxLength",20}}})},{"Values",{{"Shape",25},{"Enabled",true},{"Style","A"},{"Caption","hello"},{"PrivateField","not exported"}}}};
 auto doc=Export(page,"Shared character");assert(!doc.dump().contains("character-a")&&!doc["values"].contains("PrivateField"));doc["values"]["Shape"]=50;
 auto plan=Prepare(page,Parse(doc.dump()));assert(plan["Expected"]["Shape"]==25&&plan["Values"]["Shape"]==50&&Current(page,plan));
 auto changed=page;changed["TemplateContext"]="character-b";assert(!Current(changed,plan));changed=page;changed["Values"]["Shape"]=26;assert(!Current(changed,plan));
 auto bad=doc;bad["values"]["Shape"]=101;Reject([&]{Prepare(page,bad);});bad=doc;bad["values"]["Enabled"]="true";Reject([&]{Prepare(page,bad);});
 bad=doc;bad["values"]["Style"]="unknown";Reject([&]{Prepare(page,bad);});bad=doc;bad["values"].erase("Caption");Reject([&]{Prepare(page,bad);});
 bad=doc;bad["values"]["../file"]="payload";Reject([&]{Prepare(page,bad);});bad=doc;bad["templateVersion"]=2;Reject([&]{Prepare(page,bad);});
 bad=doc;bad["mod"]="Other";Reject([&]{Prepare(page,bad);});bad=doc;bad["script"]="execute";Reject([&]{Prepare(page,bad);});
 Reject([&]{Parse(std::string(MaximumBytes+1,'x'));});Reject([&]{Parse(std::string(18,'[')+"0"+std::string(18,']'));});
 {
   const Json rgb={{"type","text"},{"presentation","color"},{"colorAlpha",false}},rgba={{"type","text"},{"presentation","color"},{"colorAlpha",true}};
   Validate(rgb,"#abcdef");Validate(rgba,"#10203040");
   for(const Json& bad:{Json("#GG1122"),Json("#123"),Json("#11223344"),Json("112233"),Json("#112233\n"),Json(112233),Json::array({1,0,0})})Reject([&]{Validate(rgb,bad);});
   Reject([&]{Validate(rgba,"#112233");});
   const auto color=PS::ModMenuColor::Decode("#33669980");assert(color&&PS::ModMenuColor::Encode(*color,true)=="#33669980");
   auto invalid=*color;invalid[0]=std::numeric_limits<float>::quiet_NaN();assert(PS::ModMenuColor::Encode(invalid,false).empty());
   auto colors=page;colors["Settings"].push_back({{"key","Tint"},{"type","text"},{"presentation","color"},{"colorAlpha",false}});colors["Values"]["Tint"]="#112233";colors["Helpy"]["templates"]["keys"].push_back("Tint");
   auto shared=Export(colors);shared["values"]["Tint"]="#AABBCC";assert(Prepare(colors,shared)["Values"]["Tint"]=="#AABBCC");shared["values"]["Tint"]="#bad";Reject([&]{Prepare(colors,shared);});
 }
 for(const size_t count:{size_t{209},MaximumSettings}){
   auto large=page;large["Settings"]=Json::array();large["Values"]=Json::object();large["Helpy"]["templates"]["keys"]=Json::array();
   for(size_t i=0;i<count;++i){
     const auto key="Field"+std::to_string(i);
     large["Settings"].push_back({{"key",key},{"type","slider"},{"min",0},{"max",100}});
     large["Values"][key]=25;large["Helpy"]["templates"]["keys"].push_back(key);
   }
   auto shared=Export(large);assert(shared["values"].size()==count);
   const auto last="Field"+std::to_string(count-1);shared["values"][last]=50;
   const auto full=Prepare(large,Parse(shared.dump()));
   assert(full["Values"].size()==count&&full["Values"][last]==50&&Current(large,full));
   assert(full.dump().size()<=512*1024);
   auto stale=large;stale["Values"][last]=26;assert(!Current(stale,full));
   auto invalid=shared;invalid["values"][last]=101;Reject([&]{Prepare(large,invalid);});
   if(count==MaximumSettings){
     auto extraKey=large;extraKey["Helpy"]["templates"]["keys"].push_back("Overflow");Reject([&]{Contract(extraKey);});
     auto extraSetting=large;extraSetting["Settings"].push_back({{"key","Overflow"},{"type","toggle"}});Reject([&]{Settings(extraSetting);});
   }
 }
 changed=page;changed.erase("TemplateContext");Reject([&]{Export(changed);});std::cout<<"Templates: roundtrip, typed full values, private-field exclusion, bounds, identity/version, stale target and input limits passed.\n";
}
