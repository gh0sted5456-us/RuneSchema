#include "Generator/QuickMenuUI.h"
#include "Generator/HelpyStatKey.h"
#include "Generator/HelpyReferencePolicy.h"
#include "Loader/HelpyDependencyOrder.h"
#include "Loader/AssetClonePolicy.h"
#include "Runtime/HelpyBundlePublish.h"
#include "Runtime/F2CacheMigration.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace PS::QuickUI;
namespace Policy=PS::AssetMetadata;
int assertions=0;
void check(bool ok,const std::string& name){++assertions;if(!ok)throw std::runtime_error(name);}
template<class F> void rejects(F&& f,const char* name){bool threw=false;try{f();}catch(const std::exception&){threw=true;}check(threw,name);}
std::vector<Hit> hits(const Frame& f,const std::string& action){std::vector<Hit> out;for(const auto& h:f.hits)if(h.action==action)out.push_back(h);return out;}
Entry entry(int i){Entry e;e.path=e.id="/Game/Items/ITEM_"+std::to_string(i)+".ITEM_"+std::to_string(i);e.name="Example "+std::to_string(i);e.icon="/Game/Icons/T_"+std::to_string(i)+".T_"+std::to_string(i);e.cooked=true;e.cloneEligible=true;e.available=true;e.assetClass="WearableEquipmentData";e.itemTags="ItemFilter.Type.Equipment.Cape";return e;}
Model model(){Model m;Catalog c;c.authority=true;c.players.push_back({"self","Self",true});for(int i=0;i<40;++i)c.entries[0].push_back(entry(i));m.Update(c);return m;}
Model::CloneField field(std::string name,std::string type,std::string value,std::string kind){Model::CloneField f;f.name=std::move(name);f.type=std::move(type);f.value=std::move(value);f.kind=std::move(kind);f.editable=f.hasValue=true;return f;}
std::vector<Model::CloneField> fields(){auto result=std::vector<Model::CloneField>{field("FlavourText","FText","\"Old flavour\"","Text"),field("PowerLevel","int32","3","Integer"),field("Weight","float","15.0","Number"),field("bDropOnDeath","bool","true","Boolean"),field("Icon","TSoftObjectPtr<UTexture2D>","\"/Game/Icons/T_0.T_0\"","Reference"),field("DamageTypes","TArray<FGameplayTag>","[\"Slash\"]","JSON"),field("@row/WearableEquipmentDataTableRowHandle/Armour","float","5.0","Number")};result.back().scope="Stat row: DT_WearableEquipment";result.back().nativeName="Armour";return result;}
void edit(Model& m,const std::string& name,const std::string& value){m.Focus(name);auto* text=m.Field(name);check(text!=nullptr,"editable field exists: "+name);*text=value;m.Edited();m.focus.clear();}
void inspect(Model& m,int i=0){m.AcceptCloneSource(entry(i).path,fields(),"bSoftDeleted");}
void bounds(Model& m,const char* label){const auto frame=m.Render();for(const auto& h:frame.hits)check(h.box.x>=0&&h.box.y>=0&&h.box.x+h.box.w<=Width+.01f&&h.box.y+h.box.h<=Height+.01f,std::string(label)+" hit bounds");}
struct Document {int id;bool fail=false,committed=false,preserved=false;std::vector<int>* writes;void Commit(){if(fail)throw std::runtime_error("injected write failure");committed=true;writes->push_back(id);}bool Committed()const{return committed;}void PreservePending()noexcept{preserved=true;}};
int main(){try{
    // Every Boolean input combination; success is a cached reference listing, not a TTL.
    for(int mask=0;mask<32;++mask){const bool online=mask&1,present=mask&2,writable=mask&4,attempted=mask&8,manual=mask&16;
        check(PS::HelpyReferencePolicy::ShouldFetch(online,present,writable,attempted,manual)==(online&&writable&&(manual||(!present&&!attempted))),"reference request truth table");}
    check(!PS::HelpyReferencePolicy::ShouldFetch(true,true,true,false,false),"saved cache never auto-downloads");
    check(!PS::HelpyReferencePolicy::ShouldFetch(true,false,true,true,false),"failed attempt never loops in session");
    check(PS::HelpyReferencePolicy::ShouldFetch(true,true,true,true,true),"explicit update can replace saved index");
    std::set<int> codes;for(const auto& k:PS::HelpyHotkeys::Choices()){check(codes.insert(k.code).second,"unique hotkey code");check(PS::HelpyHotkeys::Find(k.name)->code==k.code,"hotkey round trip");}
    PS::HelpyHotkeys::Active=116;PS::HelpyHotkeys::SuppressedUntilRelease=0;
    check(PS::HelpyHotkeys::Matches(116)&&!PS::HelpyHotkeys::Matches(113),"rebound F5 replaces F2");
    PS::HelpyHotkeys::SuppressedUntilRelease=116;check(!PS::HelpyHotkeys::Matches(116),"rebind release guard");PS::HelpyHotkeys::SuppressedUntilRelease=0;PS::HelpyHotkeys::Capturing=true;check(!PS::HelpyHotkeys::Matches(116),"capture suppression");PS::HelpyHotkeys::Capturing=false;PS::HelpyHotkeys::Active=113;
    PS::HelpyHotkeys::Active=116;PS::HelpyHotkeys::SuppressedUntilRelease=116;
    PS::HelpyHotkeys::ObserveRelease(116,true);check(!PS::HelpyHotkeys::Matches(116),"held binding remains suppressed");
    PS::HelpyHotkeys::ObserveRelease(113,false);check(!PS::HelpyHotkeys::Matches(116),"unrelated key release cannot unlock binding");
    PS::HelpyHotkeys::ObserveRelease(116,false);check(PS::HelpyHotkeys::Matches(116),"release outside Helpy unlocks new binding");
    PS::HelpyHotkeys::Active=113;PS::HelpyHotkeys::SuppressedUntilRelease=0;
    check(!PS::HelpyHotkeys::Find("Escape")&&!PS::HelpyHotkeys::Find(27),"escape reserved for close");
    namespace V=PS::HelpyPropertyValue;
    for(const auto& text:std::vector<std::string>{"","Quote \" and \\","Two\nlines\t!",std::string("\x01\x02",2),"caf\xc3\xa9"})check(V::Unquote(V::Quote(text))==text,"text JSON roundtrip");
    check(V::Unquote("\"\\uD83D\\uDC09\"")=="\xf0\x9f\x90\x89","surrogate pair decoded");
    rejects([]{V::Unquote("\"\\uD800x\"");},"unpaired surrogate rejected");rejects([]{V::Unquote("\"\\q\"");},"bad JSON escape rejected");
    check(V::Encode("-9223372036854775808","Integer")=="-9223372036854775808","signed lower bound");
    rejects([]{V::Encode("9223372036854775808","Integer");},"integer overflow");rejects([]{V::Encode("-1","Unsigned");},"negative unsigned");rejects([]{V::Encode("nan","Number");},"nonfinite number");rejects([]{V::Encode("true-ish","Boolean");},"invalid Boolean");
    check(V::Encode("3.25","Number")=="3.25"&&V::Encode("true","Boolean")=="true","typed literal output");
    const auto stat=PS::HelpyStatKey::Make("WearableEquipmentDataTableRowHandle","Armour");check(PS::HelpyStatKey::Parse(stat)->field=="Armour","stat row key roundtrip");rejects([]{PS::HelpyStatKey::Parse("@row/A/B/C");},"nested stat key rejected");
    std::string why;Policy::Declaration d;
    check(Policy::Allowed(d,true,false,false,false,false,false,why),"native cooked source allowed");
    check(!Policy::Allowed(d,true,false,false,true,false,false,why),"cooked mod requires permission");d.safeToClone=true;
    check(Policy::Allowed(d,true,false,false,true,false,true,why),"author-opted cooked mod");
    d.cooked=true;check(!Policy::Allowed(d,false,false,false,true,false,false,why),"Cooked declaration cannot forge package proof");
    check(!Policy::Allowed(d,false,true,false,false,true,true,why),"unregistered runtime denied");
    check(!Policy::Allowed(d,false,true,true,false,false,true,why),"permanent cannot depend on session-only source");
    check(Policy::Allowed(d,false,true,true,false,false,false,why),"temporary registered runtime donor with permission");
    check(Policy::Allowed(d,false,true,true,false,true,true,why),"permanent registered installed runtime donor");
    Policy::Declaration deny;deny.safeToClone=false;deny.runeSchema=false;d=Policy::Merge(d,deny);check(d.safeToClone.has_value()&&!*d.safeToClone,"false override not lost");check(!Policy::Allowed(d,true,true,true,false,true,true,why),"explicit denial wins");
    using Node=PS::HelpyDependencies::Node;
    auto ordered=PS::HelpyDependencies::Order({Node{{"C"},"b"},Node{{"B","AliasB"},"A"},Node{{"A"},"Native"},Node{{},""}});
    check(ordered.blocked.empty()&&ordered.ordered==std::vector<std::size_t>{2,1,0,3},"child sorts behind parent with casefolding");
    ordered=PS::HelpyDependencies::Order({Node{{"A"},"B"},Node{{"B"},"A"},Node{{"C"},"A"},Node{{"D"},"Native"}});
    check(ordered.blocked==std::vector<std::size_t>{0,1,2}&&ordered.ordered==std::vector<std::size_t>{3},"cycles and descendants isolated");
    ordered=PS::HelpyDependencies::Order({Node{{"A"},"A"}});check(ordered.blocked.size()==1,"self-clone cycle");
    std::vector<Node> chain;for(int i=0;i<2048;++i)chain.push_back({{std::to_string(i)},i==2047?"native":std::to_string(i+1)});ordered=PS::HelpyDependencies::Order(chain);check(ordered.ordered.size()==chain.size()&&ordered.ordered.front()==2047&&ordered.ordered.back()==0,"deep dependencies iterative");
    for(int failure=-1;failure<4;++failure){std::vector<int>writes;std::vector<Document>docs;for(int i=0;i<4;++i)docs.push_back({i,i==failure,false,false,&writes});std::vector<Document*> ptrs;for(auto& doc:docs)ptrs.push_back(&doc);
        const auto issue=PS::HelpyBundlePublish::Publish(ptrs,[](const Document&){});check(issue.empty()==(failure<0),"publication reports exact failure");
        for(int i=0;i<4;++i){const bool done=failure<0||i<failure;check(docs[i].committed==done,"commit stops after first failure");check(docs[i].preserved==(!done),"unpublished staging preserved");}}
    {std::vector<int>writes;Document doc{0,false,false,false,&writes};const auto issue=PS::HelpyBundlePublish::Publish(std::vector<Document*>{&doc},[](const auto&){throw std::runtime_error("receipt failed");});check(!issue.empty()&&doc.committed&&!doc.preserved,"receipt failure never deletes published file");}
    auto m=model();m.cloneTab=true;m.NewDraft();inspect(m);const auto first=m.cloneId;
    edit(m,"clone-name","First item");edit(m,"clone-flavour","First flavour\nSecond line");edit(m,"clone-power","7");
    check(m.cloneEdits.at("FlavourText")==V::Quote(m.cloneFlavour)&&m.cloneEdits.at("PowerLevel")=="7","basic fields write real overrides");
    check(m.journalText==m.cloneFlavour&&!m.journalTextEdited,"journal starts from flavour");
    edit(m,"journal-text","Independent lore");edit(m,"clone-flavour","Changed flavour");check(m.journalText=="Independent lore","journal edits are independent");
    m.Activate("journal-reset");check(m.journalText=="Changed flavour"&&!m.journalTextEdited,"journal seed reset");
    m.Activate("clone-advanced");m.Activate("clone-add-field");check(m.CloneMatches().size()==1&&m.CloneMatches()[0]==fields().size()-1,"RAW property browser contains linked stat fields only");
    m.Activate("clone-scope","asset");m.Activate("clone-add-field");check(m.CloneMatches().size()==fields().size()-1,"asset property browser excludes linked RAW stats");
    m.Activate("clone-field","Weight");check(m.cloneKind=="Number"&&m.cloneValue=="15.0","typed property popup");m.cloneValue="1.25";m.Activate("clone-apply");check(m.cloneEdits.at("Weight")=="1.25"&&!m.cloneFieldOpen,"numeric row saved");
    m.Activate("clone-field","bDropOnDeath");m.Activate("clone-bool");m.Activate("clone-apply");check(m.cloneEdits.at("bDropOnDeath")=="false","Boolean row saved");
    m.Activate("clone-scope","raw");m.Activate("clone-add-field");m.Activate("clone-field",stat);m.cloneValue="19.5";m.Activate("clone-apply");check(m.cloneEdits.at(stat)=="19.5","isolated stat row override");
    m.Activate("clone-field","Weight");m.Activate("clone-inherit");check(!m.cloneEdits.contains("Weight"),"inherit removes override");
    m.appearanceMode=Model::Appearance::Copy;m.cloneAppearanceReady=true;m.cloneAppearance=entry(1).path;m.cloneAppearanceInfo=entry(1);
    check(m.EffectiveIcon()==entry(1).icon,"auto uses skin icon");m.cloneAppearanceInfo.icon.clear();check(m.EffectiveIcon()==entry(0).icon,"auto missing skin icon uses source");m.cloneAppearanceInfo=entry(1);
    edit(m,"clone-icon","/Game/Icons/Other.Other");check(m.iconMode==Model::IconMode::Override&&m.EffectiveIcon()==m.cloneIcon,"cooked icon path override wins");
    m.NewDraft();inspect(m,2);check(m.cloneId!=first&&m.cloneEdits.empty()&&m.cloneName!= "First item","new item draft isolated");const auto second=m.cloneId;
    edit(m,"clone-name","Second item");m.OpenDraft(0);check(m.cloneId==first&&m.cloneName=="First item"&&m.cloneEdits.contains(stat),"row reopens first draft intact");
    m.OpenDraft(1);check(m.cloneId==second&&m.cloneName=="Second item","row reopens second draft intact");
    m.busy=true;const auto prior=m.cloneId;m.Activate("draft-open","0");check(m.cloneId==prior,"pending command cannot switch draft");edit(m,"clone-name","Second item");m.Focus("clone-name");m.selectAll=true;m.Character('x');check(m.cloneName=="Second item","pending command blocks typing mutation");m.busy=false;m.focus.clear();
    PS::Authoring::PermanentAsset=true;m.cloneAcknowledged=true;m.makeJournal=true;m.makeRecipe=true;
    m.journalTarget={};rejects([&]{m.CloneCommand();},"journal destination required");
    m.journalTarget.path="/Game/Journal/Category.Category";m.journalTarget.grouped=true;rejects([&]{m.CloneCommand();},"journal group required");m.journalGroup="Helpy";m.journalGroupName="Helpy Items";
    rejects([&]{m.CloneCommand();},"recipe station required");m.recipeStation.path="/Game/DT_Stations.DT_Stations";m.recipeStation.row="Smithing";m.ingredients.push_back({entry(3).path,entry(3).name,entry(3).icon,"3"});
    const auto command=m.CloneCommand();check(command.makeJournal&&command.makeRecipe&&command.ingredients[0].count=="3"&&command.recipeOutput=="1","companion intent preserved in command");
    m.recipeOutput="0";rejects([&]{m.CloneCommand();},"zero output denied");m.makeRecipe=false;check(!m.CloneCommand().makeRecipe,"disabled recipe ignores stale invalid fields");
    PS::Authoring::PermanentAsset=false;rejects([&]{m.CloneCommand();},"temporary cannot install companions");PS::Authoring::PermanentAsset=true;
    m.companionsOpen=m.recipeOpen=m.ingredientPicker=true;m.lootFilter="Iron";check(m.ActiveSearch()=="Iron","nested ingredient search prioritizes real catalogue");m.ingredientPicker=false;m.iconPicker=true;check(m.ActiveSearch()=="Iron","nested icon search works");
    auto f=model();auto c=f.catalog;for(int i=0;i<40;++i){auto& e=c.entries[0][i];e.itemTags=i>=24?"ItemFilter.Type.Equipment.Weapon":"ItemFilter.Type.Equipment.Cape";e.assetClass="ItemData";e.runeSchemaManaged=i>=30;e.declaredModded=i>=30;}
    f.Update(c);f.scroll[0]=2;f.Activate("item-filter-type");f.Activate("item-filter-value","Weapons");check(f.scroll[0]==0&&f.Filtered(0).size()==16,"type filters all catalogue before paging");
    f.Activate("item-filter-source");f.Activate("item-filter-value","RuneSchema");check(f.Filtered(0).size()==10,"source and type intersect");
    f.Activate("item-filter-source");f.Activate("item-filter-value","Combined mods");check(f.Filtered(0).size()==10,"combined cooked RuneSchema provenance");
    f.ToggleFavorite(entry(39).path);f.Activate("item-subtab","favorites");check(f.Filtered(0).size()==1,"favorite/type/source compose");
    f.cloneSourcePicker=true;f.pickerType="Weapons";f.pickerSource="Cooked mods";f.lootScroll=5;check(f.LootMatches().size()==10,"picker matches same filters across full catalogue");f.ClampScroll();check(f.lootScroll==0,"picker page clamped after filters");
    c=f.catalog;c.entries[0][39].cloneEligible=false;f.Update(c);check(f.LootMatches().size()==9,"permission filter independent of source badge");
    check(hits(f.Render(),"source-clone").size()==9,"only eligible matching cards rendered");
    f.pickerFavoritesOnly=true;check(f.LootMatches().empty(),"favoriting cannot bypass clone eligibility");
    auto layout=model();layout.cloneTab=true;layout.NewDraft();inspect(layout);
    for(int state=0;state<10;++state){layout.cloneFieldOpen=layout.cloneAdvancedOpen=layout.cloneFieldPicker=layout.companionsOpen=layout.recipeOpen=layout.choicePicker=layout.helpySettingsOpen=false;layout.itemFilterMenu.clear();
        if(state==1)layout.cloneAdvancedOpen=true;
        if(state==2)layout.cloneAdvancedOpen=layout.cloneFieldPicker=true;
        if(state==3){layout.cloneFieldOpen=true;layout.cloneField="FlavourText";layout.cloneKind="Text";layout.cloneValue="More than one\nline";}
        if(state==4)layout.companionsOpen=true;
        if(state==5)layout.recipeOpen=true;
        if(state==6)layout.choicePicker=true;
        if(state==7)layout.helpySettingsOpen=true;
        if(state==8)layout.itemFilterMenu="type";
        if(state==9)layout.itemFilterMenu="source";
        bounds(layout,"Helpy modal");}
    {namespace fs=std::filesystem;const auto root=fs::current_path()/"migration-with-spaces";fs::remove_all(root);const auto old=root/"settings"/"F2-favorites.json",now=root/"runtime"/"reference"/"Helpy Favs.json";fs::create_directories(old.parent_path());{std::ofstream out(old);out<<"saved favorites";}
        auto validator=[](const fs::path& path){check(PS::F2PreferenceFile::Read(path).value()=="saved favorites","migration readback");};
        check(PS::F2CacheMigration::Prepare(now,old,validator)==PS::F2CacheMigration::Result::CopiedLegacy,"Helpy Favs path migration");check(fs::exists(old)&&fs::exists(now),"legacy file retained");
        {std::ofstream out(old);out<<"older changed";}check(PS::F2CacheMigration::Prepare(now,old,validator)==PS::F2CacheMigration::Result::CurrentExists,"new favorite location wins");check(PS::F2PreferenceFile::Read(now).value()=="saved favorites","existing current favorites untouched");fs::remove_all(root);}
    std::cout<<"PASS: "<<assertions<<" Helpy v9 policy, draft, field, filter, dependency, publication and migration assertions. No Unreal/Windows execution.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<assertions<<": "<<e.what()<<'\n';return 1;}}
