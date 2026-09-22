#include "Loader/NpcCatalog.h"
#include "Loader/NpcNetwork.h"
#include "Loader/NpcIdentityPayload.h"
#include "Loader/NpcComponentSelection.h"
#include "Loader/NpcCollisionArguments.h"
#include "Loader/NpcPlacement.h"
#include "Loader/VendorIdentity.h"
#include "Loader/VendorOffers.h"
#include <cassert>
#include <type_traits>
#include <fstream>
#include <filesystem>
using DragonWilds::NpcCatalog;
using Json=nlohmann::json;
template<class F> bool Rejects(F fn){try{fn();}catch(const std::exception&){return true;}return false;}
int main(int argc,char** argv){
    {
        Json prop={{"Id","table"},{"Type","Prop"},{"Mesh","/Engine/BasicShapes/Cube.Cube"},
            {"Location",Json::array({0,0,0})},{"NoInteract",true},{"HideName",true},{"EnableCollision",true},
            {"VendorID","missing_shop"},{"DialogueID","missing_dialogue"}};
        NpcCatalog catalog;catalog.AddNpc("test",prop);
        const auto resolved=catalog.Resolve().at(0).Data;
        assert(resolved.at("Stage")=="Visual" && resolved.at("EnableCollision")==true);
        assert(!resolved.contains("VendorID") && !resolved.contains("DialogueID"));
        assert(resolved.at("HideName")==true);
        for(const auto* field:{"NoInteract","HideName"}) {
            auto bad=prop;bad[field]="true";
            assert(Rejects([&]{NpcCatalog c;c.AddNpc("test",bad);}));
        }
        prop["NoInteract"]=false;
        NpcCatalog active;active.AddNpc("test",prop);
        assert(Rejects([&]{active.Resolve();}));
        assert(DragonWilds::NpcNetwork::Supported(true,false,true,false,false,false,false,true));
        assert(!DragonWilds::NpcNetwork::Supported(false,false,true,false,false,false,false,true));
        prop["NoInteract"]=true;
        for(const auto* phase:{"Any","Day","Night"}) {
            auto timed=prop;timed["Id"]=std::string("table-")+phase;timed["TimeOfDay"]=phase;
            NpcCatalog timedCatalog;timedCatalog.AddNpc("test",timed);
            assert(timedCatalog.Resolve().at(0).Data.at("TimeOfDay")==phase);
        }
        for(const auto& phase:{Json("Dusk"),Json(7),Json(false)}) {
            auto timed=prop;timed["Id"]="bad-time";timed["TimeOfDay"]=phase;
            assert(Rejects([&]{NpcCatalog timedCatalog;timedCatalog.AddNpc("test",timed);}));
        }
    }
    const Json glow={{"Type","Ghost Glow"},{"MainColor",{{"R",0.45},{"G",0.15},{"B",0.95},{"A",1.0}}}};
    DragonWilds::NpcVisualEffect::Validate(glow);
    for(const auto& bad:std::vector<Json>{Json::array(),{{"Type","Ghost"}},{{"Type","Ghost Glow"},{"Unknown",true}},
        {{"Type","Ghost Glow"},{"Overlay",false},{"BodyMaterial",false}},
        {{"Type","Ghost Glow"},{"Overlay",1}},{{"Type","Ghost Glow"},{"MainColor",{{"R",1}}}}})
        assert(Rejects([&]{DragonWilds::NpcVisualEffect::Validate(bad);}));
    for(const auto* channel:{"R","G","B","A"}) {
        auto bad=glow;bad["MainColor"][channel]=-1;
        assert(Rejects([&]{DragonWilds::NpcVisualEffect::Validate(bad);}));
        bad["MainColor"][channel]=65;
        assert(Rejects([&]{DragonWilds::NpcVisualEffect::Validate(bad);}));
    }
    const Json gameplay={{"Id","rock"},{"VendorID","armory"},{"Items",Json::array({{{"Price",5}}})}};
    const auto compatible=DragonWilds::NpcNetwork::GameplayFingerprint("mod",gameplay);
    auto cosmetic=gameplay;
    cosmetic["VendorHeaderImage"]="/Game/Custom/Banner.Banner";
    cosmetic["DisplayName"]="Localized rock";
    cosmetic["MerchantName"]="Localized shop";
    cosmetic["Map"]={{"Enabled",false}};
    cosmetic["OverheadIcon"]={{"Enabled",false}};
    cosmetic["VisualEffect"]=glow;
    assert(DragonWilds::NpcNetwork::GameplayFingerprint("mod",cosmetic)==compatible);
    cosmetic["Items"][0]["Price"]=6;
    assert(DragonWilds::NpcNetwork::GameplayFingerprint("mod",cosmetic)!=compatible);
    assert(DragonWilds::NpcNetwork::GameplayFingerprint("other",gameplay)!=compatible);
    auto timed=gameplay;timed["TimeOfDay"]="Night";
    assert(DragonWilds::NpcNetwork::GameplayFingerprint("mod",timed)!=compatible);
    for(const auto* key:{"Id","VendorID","DialogueID","RequiresFlag","Mesh","Location","VendorProperties"}) {
        auto changed=gameplay; changed[key]="different";
        assert(DragonWilds::NpcNetwork::GameplayFingerprint("mod",changed)!=compatible);
    }
    const auto identityText=DragonWilds::NpcIdentity::Encode("ExampleMod","cow", "revision1");
    const auto decoded=DragonWilds::NpcIdentity::Decode(identityText);
    assert(decoded.Mod=="ExampleMod" && decoded.Npc=="cow" && decoded.Fingerprint=="revision1");
    assert(Rejects([]{DragonWilds::NpcIdentity::Decode("");}));
    assert(Rejects([]{DragonWilds::NpcIdentity::Decode(std::string(4097,'x'));}));
    for(const auto& bad:std::vector<Json>{
        Json::array(),{{"version",2},{"mod","M"},{"npc","n"},{"fingerprint","r"}},
        {{"version",1.0},{"mod","M"},{"npc","n"},{"fingerprint","r"}},
        {{"version",1},{"mod","M"},{"npc",""},{"fingerprint","r"}},
        {{"version",1},{"mod","M"},{"npc",12},{"fingerprint","r"}},
        {{"version",1},{"mod","M"},{"npc","n"}},
        {{"version",1},{"mod","M"},{"npc","n"},{"fingerprint","r"},{"extra",true}}})
        assert(Rejects([&]{DragonWilds::NpcIdentity::Decode(bad.dump());}));
    assert(Rejects([]{DragonWilds::NpcIdentity::Encode("bad\nmod","n","r");}));
    assert(Rejects([]{DragonWilds::NpcIdentity::Encode(std::string(257,'x'),"n","r");}));
    assert(DragonWilds::VendorIdentity::RetirementConfirmed(false,false));
    assert(DragonWilds::VendorIdentity::RetirementConfirmed(false,true));
    assert(DragonWilds::VendorIdentity::RetirementConfirmed(true,false));
    assert(!DragonWilds::VendorIdentity::RetirementConfirmed(true,true));
    for(int interactions=0;interactions<=2;++interactions)
        for(int stations=0;stations<=2;++stations)
            assert(DragonWilds::NpcNetwork::ClientComponentsReady(interactions,stations)==(interactions==1 && stations==1));
    assert(DragonWilds::NpcNetwork::ClientArrivalAttempts==40);
    if(argc==2) {
        const std::filesystem::path root=argv[1];
        std::ifstream npcFile(root/"npc/00-Cow.jsonc"),shopFile(root/"vendors/00-Shop.json");
        assert(npcFile && shopFile);
        const auto npc=Json::parse(npcFile,nullptr,true,true),shop=Json::parse(shopFile);
        NpcCatalog packaged;packaged.AddStore("RuneSchemaMultiplayerTest",shop);packaged.AddNpc("RuneSchemaMultiplayerTest",npc);
        const auto definitions=packaged.Resolve();
        assert(definitions.size()==1 && definitions[0].Data.at("Multiplayer")==true);
        assert(definitions[0].Data.at("Stage")=="Merchant");
        assert(!definitions[0].Data.contains("DialogueID") && !definitions[0].Data.contains("RequiresFlag"));
        assert(definitions[0].Data.at("Items").size()==1);
    }
    for (const bool enabled : {false, true}) {
        const auto mode=DragonWilds::NpcCollision::Enabled(enabled);
        static_assert(std::is_same_v<std::remove_cv_t<decltype(mode)>,uint8_t>);
        assert(mode== (enabled ? 1 : 0));
        const auto response=DragonWilds::NpcCollision::PawnResponse(enabled);
        static_assert(std::is_same_v<decltype(response.Channel),uint8_t>);
        static_assert(std::is_same_v<decltype(response.NewResponse),uint8_t>);
        assert(response.Channel==2 && response.NewResponse== (enabled ? 2 : 0));
    }
    int firstMesh=0,secondMesh=0;
    using DragonWilds::NpcComponents::Candidate;
    using DragonWilds::NpcComponents::Select;
    assert(Select(std::vector<Candidate<int>>{})==nullptr);
    assert(Select(std::vector<Candidate<int>>{{&firstMesh,true,true,false}})==&firstMesh);
    assert(Select(std::vector<Candidate<int>>{{&firstMesh,true,true,false},{&secondMesh,true,true,true}})==&secondMesh);
    assert(Select(std::vector<Candidate<int>>{{&firstMesh,false,true,true},{&secondMesh,true,true,false}})==&secondMesh);
    assert(Select(std::vector<Candidate<int>>{{&firstMesh,true,false,true},{nullptr,true,true,true}})==nullptr);
    assert(Rejects([&]{Select(std::vector<Candidate<int>>{{&firstMesh,true,true,false},{&secondMesh,true,true,false}});}));
    assert(Rejects([&]{Select(std::vector<Candidate<int>>{{&firstMesh,true,true,true},{&secondMesh,true,true,true}});}));
    int retired=0,replacement=0;
    using DragonWilds::VendorIdentity::RetiredInstanceMatches;
    assert(RetiredInstanceMatches(&retired,&retired,42,42,7,7,true));
    assert(!RetiredInstanceMatches(&retired,&replacement,42,43,7,8,true));
    assert(!RetiredInstanceMatches(&retired,&retired,42,42,7,8,true));
    assert(!RetiredInstanceMatches(&retired,&retired,42,43,7,7,true));
    assert(!RetiredInstanceMatches(&retired,&retired,42,42,7,7,false));
    assert(!RetiredInstanceMatches(nullptr,nullptr,42,42,7,7,true));
    assert(!RetiredInstanceMatches(&retired,&retired,-1,-1,7,7,true));
    assert(RetiredInstanceMatches(&retired,&retired,42,42,0,0,true));
    using DragonWilds::HumanNpc::SelectAttachment;
    const auto bridgeExists=[](const std::string& name){return name=="RS_MainHand" || name=="RS_OffHand" || name=="hand_r" || name=="hand_l";};
    assert(SelectAttachment("None","Right",bridgeExists)=="RS_MainHand");
    assert(SelectAttachment("missing","Left",bridgeExists)=="RS_OffHand");
    const auto boneExists=[](const std::string& name){return name=="hand_r" || name=="hand_l";};
    assert(SelectAttachment("None","Right",boneExists)=="hand_r");
    assert(SelectAttachment("missing","Left",boneExists)=="hand_l");
    const auto propExists=[](const std::string& name){return name=="prop_r" || name=="prop_l";};
    assert(SelectAttachment("None","Right",propExists)=="prop_r");
    assert(SelectAttachment("missing","Left",propExists)=="prop_l");
    assert(SelectAttachment("prop_l","Right",propExists)=="prop_l");
    assert(SelectAttachment("","Unknown",propExists).empty());
    assert(SelectAttachment("None","Right",[](const std::string&){return false;}).empty());
    bool warned=false,continued=false;
    DragonWilds::HumanNpc::ApplyOptionalHeldVisual([]{throw std::runtime_error("missing socket");},
        [&](const char* error){warned=std::string(error)=="missing socket";});
    continued=true;
    assert(warned && continued);
    warned=false;
    DragonWilds::HumanNpc::ApplyOptionalHeldVisual([]{},[&](const char*){warned=true;});
    assert(!warned);
    assert(DragonWilds::HumanNpc::IsMainHandSlot("HeldOnlyRight"));
    assert(DragonWilds::HumanNpc::IsMainHandSlot("HeldTwoHanded"));
    assert(!DragonWilds::HumanNpc::IsMainHandSlot("HeldOnlyLeft"));
    assert(DragonWilds::HumanNpc::IsOffHandSlot("HeldOnlyLeft"));
    assert(!DragonWilds::HumanNpc::IsOffHandSlot("HeldOnlyRight"));
    assert(!DragonWilds::HumanNpc::IsMainHandSlot("Head"));
    assert(!DragonWilds::HumanNpc::IsMainHandSlot("HeldRightHand"));
    Json human={{"Id","human"},{"Type","Human"},{"Location",{1,2,3}},{"Appearance",Json::object()},
        {"Equipment",{{"Body","/Game/Test/Robes.Robes"},{"MainHand","/Game/Test/Staff.Staff"}}}};
    Json prop={{"Id","book"},{"Type","Prop"},{"Mesh","/Game/Test/Book.Book"},{"Location",{1,2,3}}};
    NpcCatalog props;props.AddNpc("Test",prop);
    assert(props.Resolve()[0].Data["Type"]=="Prop");
    assert(DragonWilds::HumanNpc::UsesStaticMesh(prop));
    assert(!DragonWilds::HumanNpc::IsHuman(prop));
    auto loreProp=prop;loreProp["LoreEntry"]="RuneSchema_TestLore";loreProp["Multiplayer"]=true;
    NpcCatalog books;books.AddNpc("Test",loreProp);
    const auto book=books.Resolve()[0].Data;
    assert(book["Stage"]=="Interaction" && book["InteractionProperties"]["InteractionPrompt"]=="Examine");
    assert(book["LoreEntry"]=="RuneSchema_TestLore");
    for(const auto* field:{"DialogueID","VendorID"}) {
        auto conflict=loreProp;conflict[field]="other";
        assert(Rejects([&]{NpcCatalog bad;bad.AddNpc("Test",conflict);}));
    }
    for(const auto& value:{Json(""),Json(1),Json("bad\nentry")}) {
        auto invalid=loreProp;invalid["LoreEntry"]=value;
        assert(Rejects([&]{NpcCatalog bad;bad.AddNpc("Test",invalid);}));
    }
    for(int interactions=0;interactions<3;++interactions)for(int stations=0;stations<3;++stations)
        assert(DragonWilds::NpcNetwork::ClientComponentsReady(interactions,stations,false)==(interactions==1 && stations==0));
    auto badProp=prop;badProp.erase("Mesh");assert(Rejects([&]{DragonWilds::HumanNpc::Validate(badProp);}));
    for(const auto* field:{"VisualSource","IdleAnimation","Pose","Equipment"}) {
        badProp=prop;badProp[field]="invalid";assert(Rejects([&]{DragonWilds::HumanNpc::Validate(badProp);}));
    }
    for(const auto* key:DragonWilds::HumanNpc::AppearanceKeys)human["Appearance"][key]="row";
    NpcCatalog humans;humans.AddNpc("Test",human);
    assert(humans.Resolve()[0].Data["Type"]=="Human");
    assert(humans.Resolve()[0].Data["Equipment"]["MainHand"]=="/Game/Test/Staff.Staff");
    for(const auto* key:DragonWilds::HumanNpc::AppearanceKeys){auto bad=human;bad["Appearance"].erase(key);assert(Rejects([&]{DragonWilds::HumanNpc::Validate(bad);}));}
    auto badHuman=human;badHuman["Type"]="AI";assert(Rejects([&]{DragonWilds::HumanNpc::Validate(badHuman);}));
    badHuman=human;badHuman["Mesh"]="/Test/Cow";assert(Rejects([&]{DragonWilds::HumanNpc::Validate(badHuman);}));
    badHuman=human;badHuman["Equipment"]["MainHand"]="Staff";assert(Rejects([&]{DragonWilds::HumanNpc::Validate(badHuman);}));
    badHuman=human;badHuman["Equipment"]["InventedSlot"]="/Test/Item";assert(Rejects([&]{DragonWilds::HumanNpc::Validate(badHuman);}));
    badHuman=human;badHuman["Type"]="Player";assert(Rejects([&]{DragonWilds::HumanNpc::Validate(badHuman);}));
    using DragonWilds::NpcPlacement;
    const double position[3]={1,2,3},rotation[3]={0,0,0};
    assert(NpcPlacement::Matches(position,{1,2,3}));
    assert(NpcPlacement::Matches(position,{1.05,2,3}));
    assert(!NpcPlacement::Matches(position,{1,2,3.2}));
    assert(NpcPlacement::Matches(rotation,{0,360,-360},true));
    assert(!NpcPlacement::Matches(rotation,{0,1,0},true));
    assert(!NpcPlacement::Matches(position,{1,2,std::numeric_limits<double>::quiet_NaN()}));
    assert(NpcPlacement::Parse(Json{{"Z","$"},{"Y",2},{"X",1}}).Ground);
    assert(NpcPlacement::Parse(Json::array({1,2,"$+90"})).Offset==90);
    assert(NpcPlacement::Parse(Json{{"X",1},{"Y",2},{"Z","$-2.5"}}).Offset==-2.5);
    assert(!NpcPlacement::Parse(Json::array({1,2,3})).Ground);
    for(const auto& invalid:Json::array({"$1","$+","$+nan","$+100001","$-inf","$+2junk"}))
        assert(Rejects([&]{NpcPlacement::Parse(Json::array({1,2,invalid}));}));
    assert(Rejects([]{NpcPlacement::Parse(Json{{"X",1},{"Y",2}});}));
    assert(Rejects([]{NpcPlacement::Parse(Json::array({"$",2,3}));}));
    const Json cow={{"Id","cow"},{"Mesh","/Test/Cow"},{"Location",{1,2,3}},{"DisplayName","Cow"},{"VendorID","guild"}};
    auto second=cow;second["Id"]="clerk";second["VendorID"]="Mod:guild";
    const Json shop={{"Id","guild"},{"Items",Json::array()},{"MerchantName","Guild"}};
    auto legacy=shop;legacy["Npcs"]={"cow"};assert(Rejects([&]{NpcCatalog old;old.AddStore("Mod",legacy);}));
    for(const auto& ref:Json::array({"",":guild","Mod:","Mod:guild:bad",nullptr,4})) {
        auto bad=cow;bad["VendorID"]=ref;assert(Rejects([&]{NpcCatalog invalid;invalid.AddNpc("Mod",bad);}));
    }
    NpcCatalog catalog;
    auto bannerShop=shop;bannerShop["VendorHeaderImage"]="/MyMod/UI/T_Banner.T_Banner";
    NpcCatalog banners;banners.AddNpc("Mod",cow);banners.AddNpc("Other",second);banners.AddStore("Mod",bannerShop);
    const auto withBanner=banners.Resolve();
    assert(withBanner[0].Data["VendorHeaderImage"]==bannerShop["VendorHeaderImage"]);
    assert(withBanner[1].Data["VendorHeaderImage"]==bannerShop["VendorHeaderImage"]);
    for(const auto& bad:Json::array({nullptr,42,"banner.png","/Bad\nPath"})) {
        auto broken=shop;broken["VendorHeaderImage"]=bad;
        assert(Rejects([&]{NpcCatalog invalid;invalid.AddStore("Mod",broken);}));
    }
    assert(NpcCatalog::HeaderImage(shop)==NpcCatalog::DefaultHeaderImage);
    auto blankBanner=shop;blankBanner["VendorHeaderImage"]="";
    assert(NpcCatalog::HeaderImage(blankBanner)==NpcCatalog::DefaultHeaderImage);
    auto repairShop=shop;repairShop["Repairable"]=true;
    repairShop["Masterworkable"]=true;
    NpcCatalog repairs;repairs.AddStore("Mod",repairShop);repairs.AddNpc("Mod",cow);
    assert(repairs.Resolve()[0].Data["Repairable"]==true);
    assert(repairs.Resolve()[0].Data["Masterworkable"]==true);
    auto invalidRepair=shop;invalidRepair["Repairable"]="yes";
    assert(Rejects([&]{NpcCatalog invalid;invalid.AddStore("Mod",invalidRepair);}));
    auto gatedShop=shop;
    gatedShop["CategoryRules"]=Json::array({{{"Category","Vault"},{"MinPowerLevel",5}},
        {{"Category","Night Stock"},{"TimeOfDay","Night"}}});
    NpcCatalog categoryCatalog;categoryCatalog.AddStore("Mod",gatedShop);categoryCatalog.AddNpc("Mod",cow);
    const auto categoryData=categoryCatalog.Resolve()[0].Data;
    assert(categoryData["CategoryRules"].size()==2);
    const auto rules=DragonWilds::VendorCategoryGate::Parse(categoryData["CategoryRules"]);
    Json offers=Json::array({{{"Item","/I"},{"Currency","/C"},{"Price",1},{"Category","Vault"}},
        {{"Item","/I"},{"Currency","/C"},{"Price",1},{"Category","Night Stock"}},
        {{"Item","/I"},{"Currency","/C"},{"Price",1},{"Category","Always"}}});
    assert(DragonWilds::VendorCategoryGate::Filter(offers,rules,4,DragonWilds::TimeOfDay::Requirement::Night).size()==2);
    assert(DragonWilds::VendorCategoryGate::Filter(offers,rules,5,DragonWilds::TimeOfDay::Requirement::Day).size()==2);
    assert(DragonWilds::VendorCategoryGate::Filter(offers,rules,std::nullopt,DragonWilds::TimeOfDay::Requirement::Day).size()==1);
    auto duplicateRule=gatedShop;duplicateRule["CategoryRules"].push_back({{"Category","Vault"},{"TimeOfDay","Day"}});
    assert(Rejects([&]{NpcCatalog invalid;invalid.AddStore("Mod",duplicateRule);}));
    catalog.AddStore("Mod",shop); // References resolve after all mod files load.
    catalog.AddNpc("Mod",cow);catalog.AddNpc("Other",second);
    assert(catalog.ResolveNpcReference("Mod","cow")=="Mod:cow");
    assert(catalog.ResolveNpcReference("Mod","Other:clerk")=="Other:clerk");
    assert(catalog.ContainsNpcReference("Other","clerk") && !catalog.ContainsNpcReference("Mod","missing"));
    const auto result=catalog.Resolve();
    assert(result.size()==2 && result[0].StoreOwner==result[1].StoreOwner);
    assert(result[0].Data["LoaderID"]=="Mod:cow" && result[1].Data["LoaderID"]=="Other:clerk");
    assert(result[0].Data["References"]["Vendor"]=="Mod:guild");
    assert(result[0].Data["Stage"]=="Merchant" && result[0].Data["DisplayName"]=="Cow");
    assert(result[0].Data["Id"]=="cow" && result[0].Data["MerchantName"]=="Guild");
    const Json offer={{"VendorID","Mod:guild"},{"Item","/Test/Cabbage"},{"Currency","/Test/Coin"},{"Price",2}};
    const auto contribution=NpcCatalog::ParseStoreOffer("Addon","cabbage",offer);
    const auto extended=catalog.Resolve({contribution});
    assert(extended[0].Data["Items"]==extended[1].Data["Items"]);
    assert(extended[0].Data["Items"].size()==1);
    assert(extended[0].Data["Items"][0]["_RecipeSlot"]=="recipe:Addon:cabbage");
    assert(catalog.Resolve()[0].Data["Items"].empty());
    assert(catalog.Resolve({contribution})[0].Data==extended[0].Data);
    auto multi=offer;
    multi["RuneSchemaVendors"]={"Mod:guild","guild","Mod:guild","Other:branch"};
    multi["VanillaVendors"]={{{"Table","DT_VendorDataTable"},{"Row","TestRow"}},{{"Table","dt_vendordatatable"},{"Row","testrow"}}};
    assert(NpcCatalog::StoreTargets("Mod",multi).size()==2);
    assert(NpcCatalog::VanillaTargets(multi).size()==1);
    NpcCatalog branches;
    branches.AddStore("Mod",shop);
    auto branch=shop;branch["Id"]="branch";branches.AddStore("Other",branch);
    branches.AddNpc("Mod",cow);
    auto clerk=cow;clerk["VendorID"]="Other:branch";branches.AddNpc("Other",clerk);
    const auto both=branches.Resolve({NpcCatalog::ParseStoreOffer("Mod","both",multi)});
    assert(both.size()==2 && both[0].Data["Items"].size()==1 && both[1].Data["Items"].size()==1);
    assert(both[0].Data["Items"][0]==both[1].Data["Items"][0]);
    assert(!both[0].Data["Items"][0].contains("VanillaVendors"));
    auto nativeOnly=multi;nativeOnly.erase("VendorID");nativeOnly["RuneSchemaVendors"]=Json::array();
    assert(catalog.Resolve({NpcCatalog::ParseStoreOffer("Mod","native",nativeOnly)})[0].Data["Items"].empty());
    auto noTargets=nativeOnly;noTargets["VanillaVendors"]=Json::array();
    assert(Rejects([&]{NpcCatalog::ParseStoreOffer("Mod","empty",noTargets);}));
    for(const auto value:{Json("guild"),Json::array({42})}) {
        auto bad=multi;bad["RuneSchemaVendors"]=value;
        assert(Rejects([&]{NpcCatalog::ParseStoreOffer("Mod","bad",bad);}));
    }
    auto badTarget=multi;badTarget["VanillaVendors"][0]["Replaces"]="Anything";
    assert(Rejects([&]{NpcCatalog::ParseStoreOffer("Mod","bad",badTarget);}));
    for(const auto* mode:{"Pawn","Native","None"}) {
        auto configured=cow;configured["MeshCollision"]=mode;
        NpcCatalog accepted;accepted.AddNpc("Mod",configured);
    }
    auto badCollision=cow;badCollision["MeshCollision"]="Ragdoll";
    assert(Rejects([&]{NpcCatalog rejected;rejected.AddNpc("Mod",badCollision);}));
    assert(Rejects([&]{catalog.Resolve({contribution,contribution});}));
    auto invalid=offer;invalid["Unlock"]=true;
    assert(Rejects([&]{NpcCatalog::ParseStoreOffer("Addon","cabbage",invalid);}));
    invalid=offer;invalid["VendorID"]="Missing";
    assert(Rejects([&]{catalog.Resolve({NpcCatalog::ParseStoreOffer("Addon","cabbage",invalid)});}));
    invalid=offer;invalid["VendorID"]="Mod:guild:extra";
    assert(Rejects([&]{NpcCatalog::ParseStoreOffer("Addon","cabbage",invalid);}));
    invalid=offer;invalid["Price"]=-1;
    assert(Rejects([&]{NpcCatalog::ParseStoreOffer("Addon","cabbage",invalid);}));
    invalid=offer;invalid["VendorID"]="guild";
    assert(catalog.Resolve({NpcCatalog::ParseStoreOffer("Mod","local",invalid)})[0].Data["Items"].size()==1);
    assert(Rejects([&]{catalog.AddNpc("Mod",cow);}));
    assert(Rejects([&]{catalog.AddStore("Mod",shop);}));
    NpcCatalog missing;missing.AddNpc("Mod",cow);assert(Rejects([&]{missing.Resolve();}));
    auto visual=cow;visual.erase("VendorID");
    NpcCatalog neutral;neutral.AddNpc("Mod",visual);
    assert(neutral.Resolve()[0].Data["Stage"]=="Visual");
    auto unsupported=cow;unsupported["Equipment"]=Json::object();
    assert(Rejects([&]{neutral.AddNpc("Other",unsupported);}));
    auto disabled=shop;disabled["Enabled"]=false;
    auto gated=shop;gated["RequiresFlag"]="heard";gated["LockedDialogueID"]="locked";
    assert(Rejects([&]{NpcCatalog invalidCatalog;invalidCatalog.AddStore("Mod",gated);}));
    auto placedStore=shop;placedStore["Location"]={0,0,0};
    assert(Rejects([&]{NpcCatalog invalidCatalog;invalidCatalog.AddStore("Mod",placedStore);}));
    auto talk=visual;talk["DialogueID"]="story";NpcCatalog dialogue;dialogue.AddNpc("Mod",talk);
    assert(dialogue.Resolve()[0].Data["Stage"]=="Interaction");
    assert(dialogue.Resolve()[0].Data["References"]["Dialogue"]=="Mod:story");
    assert(dialogue.Resolve()[0].Data["InteractionProperties"]["InteractionPrompt"]=="Talk");
    talk["VendorID"]="guild";NpcCatalog talkingShop;talkingShop.AddStore("Mod",shop);talkingShop.AddNpc("Mod",talk);
    assert(talkingShop.Resolve()[0].Data["InteractionProperties"]["InteractionPrompt"]=="Talk");
    assert(talkingShop.Resolve()[0].StoreOwner=="store:Mod:guild");
    neutral.AddStore("Mod",disabled);assert(neutral.Resolve()[0].Data["Stage"]=="Visual");
    auto linked=visual;linked["LoreID"]="Other:history";linked["QuestID"]="welcome";
    NpcCatalog links;links.AddNpc("Mod",linked);const auto linkedResult=links.Resolve()[0].Data;
    assert(linkedResult["LoreEntry"]=="Other:history" && linkedResult["QuestID"]=="Mod:welcome");
    assert(linkedResult["References"]["Lore"]=="Other:history" && linkedResult["References"]["Quest"]=="Mod:welcome");
    auto duplicateLore=linked;duplicateLore["LoreEntry"]="legacy";
    assert(Rejects([&]{NpcCatalog invalidCatalog;invalidCatalog.AddNpc("Mod",duplicateLore);}));
    NpcCatalog isolated;isolated.AddStore("Mod",shop);isolated.AddNpc("Mod",cow);
    auto orphan=visual;orphan["Id"]="orphan";orphan["VendorID"]="missing";isolated.AddNpc("Mod",orphan);
    std::vector<std::string> isolatedErrors;
    const auto survivors=isolated.ResolveIsolated({},[&](const auto& key,const auto& error){isolatedErrors.push_back(key+":"+error);});
    assert(survivors.size()==1 && survivors[0].Data["LoaderID"]=="Mod:cow" && isolatedErrors.size()==1);
    auto conflict=shop;conflict["Id"]="second";
    catalog.AddStore("Mod",conflict);assert(catalog.Resolve()[0].StoreOwner==result[0].StoreOwner);
    NpcCatalog disabledTarget;disabledTarget.AddNpc("Mod",cow);disabledTarget.AddStore("Mod",disabled);
    assert(disabledTarget.Resolve()[0].Data["Stage"]=="Visual");
    auto marked=visual;marked["Map"]={{"Enabled",true}};marked["OverheadIcon"]={{"Enabled",true},{"Height",220},{"Scale",0.35},{"Distance",5000}};
    NpcCatalog markers;markers.AddNpc("Markers",marked);assert(markers.Resolve()[0].Data["Map"]["Enabled"]==true);
    marked["Map"]["ShowName"]=false;
    NpcCatalog unnamed;unnamed.AddNpc("Unnamed",marked);
    assert(unnamed.Resolve()[0].Data["Map"]["ShowName"]==false);
    assert(DragonWilds::NpcMarkers::MapLabel(Json::object(),"Jonesy")=="Jonesy");
    assert(DragonWilds::NpcMarkers::MapLabel({{"ShowName",true}},"Jonesy")=="Jonesy");
    assert(DragonWilds::NpcMarkers::MapLabel({{"ShowName",false}},"Jonesy").empty());
    for(const auto& option:{Json("false"),Json(0),Json(),Json::object()}) {
        auto bad=marked;bad["Map"]["ShowName"]=option;NpcCatalog invalidCatalog;
        assert(Rejects([&]{invalidCatalog.AddNpc("BadLabel",bad);}));
    }
    auto invalidOverhead=marked;invalidOverhead["OverheadIcon"]["ShowName"]=false;
    assert(Rejects([&]{NpcCatalog invalidCatalog;invalidCatalog.AddNpc("BadOverhead",invalidOverhead);}));
    for(const auto& bad:Json::array({{{"Map",true}},{{"Map",{{"Enabled","true"}}}},{{"Map",{{"Icon","relative"}}}},{{"Map",{{"Height",1}}}},{{"OverheadIcon",{{"Scale",0}}}},{{"OverheadIcon",{{"Distance",-1}}}}})) {
        auto test=visual;for(const auto& [key,value]:bad.items())test[key]=value;
        assert(Rejects([&]{NpcCatalog invalidCatalog;invalidCatalog.AddNpc("Invalid",test);}));
    }
    auto resource=visual;resource["Type"]="Resource";resource.erase("VisualSource");resource["Mesh"]="/Game/Stone.Stone";
    for(auto candidate:std::vector<Json>{visual,human,resource,prop}) {
        candidate["VisualEffect"]=glow;
        NpcCatalog effects;effects.AddNpc("Effects",candidate);
        assert(effects.Resolve()[0].Data["VisualEffect"]==glow);
    }
    NpcCatalog rocks;rocks.AddNpc("Resources",resource);assert(rocks.Resolve()[0].Data["Type"]=="Resource");
    for(const auto& field:{"VisualSource","IdleAnimation","Appearance","Equipment","Pose","Ghost"}) {
        auto invalidResource=resource;invalidResource[field]="/Game/Invalid.Invalid";
        assert(Rejects([&]{NpcCatalog c;c.AddNpc("Invalid",invalidResource);}));
    }
    auto missingMesh=resource;missingMesh.erase("Mesh");
    assert(Rejects([&]{NpcCatalog c;c.AddNpc("Invalid",missingMesh);}));
    // Identity is NPC-owned, not derived from which store is bound.
    assert(DragonWilds::NpcNetwork::ActorName("mod:npc")==DragonWilds::NpcNetwork::ActorName("mod:npc"));
    assert(DragonWilds::NpcNetwork::NeedsIdentityReplacement(true,"BP_BaseInteractableNPC_C_42","RuneSchemaNPC_expected"));
    assert(!DragonWilds::NpcNetwork::NeedsIdentityReplacement(true,"same","same"));
    assert(!DragonWilds::NpcNetwork::NeedsIdentityReplacement(false,"legacy","expected"));
    assert(!DragonWilds::NpcNetwork::NeedsIdentityReplacement(true,"legacy",""));
    assert(DragonWilds::NpcNetwork::ActorName("mod:npc")!=DragonWilds::NpcNetwork::ActorName("other:npc"));
    assert(std::wstring_view(DragonWilds::NpcIdentity::ClassPath).starts_with(L"/RuneSchema/Networking/"));
    assert(std::wstring_view(DragonWilds::NpcIdentity::LegacyClassPath).starts_with(L"/Game/Mods/RuneSchema/Networking/"));
    assert(DragonWilds::NpcNetwork::Supported(true,false,false,true,false,false));
    assert(!DragonWilds::NpcNetwork::Supported(false,false,false,true,false,false));
    assert(DragonWilds::NpcNetwork::Supported(true,true,false,true,false,false));
    human["Equipment"]["Head"]="JSlAV05XOfg2HBO7L7dkqw";
    DragonWilds::HumanNpc::Validate(human);
    assert(DragonWilds::NpcNetwork::Supported(true,false,true,true,false,false));
    assert(DragonWilds::NpcNetwork::Supported(true,false,false,true,true,false));
    assert(DragonWilds::NpcNetwork::Supported(true,false,false,true,false,true));
    assert(DragonWilds::NpcNetwork::Supported(true,false,true,true,true,true));
    assert(DragonWilds::NpcNetwork::Supported(true,false,true,false,true,false));
    assert(DragonWilds::NpcNetwork::Supported(true,false,true,false,false,false,true));
    assert(!DragonWilds::NpcNetwork::Supported(false,false,true,false,false,false,true));
    assert(!DragonWilds::NpcNetwork::Supported(true,false,true,false,false,false));
    const Json graphs={{"mod:story",{{"Entry","hello"}}}};
    const Json manifest=Json::array({{{"Key","mod:quest"},{"NetId",300},{"Definition",{{"Id","quest"}}}}});
    const auto dialogueFingerprint=DragonWilds::NpcNetwork::DialogueFingerprint("base",graphs,manifest);
    assert(dialogueFingerprint==DragonWilds::NpcNetwork::DialogueFingerprint("base",Json::parse(graphs.dump()),Json::parse(manifest.dump())));
    auto otherIds=manifest;otherIds[0]["NetId"]=301;
    assert(dialogueFingerprint!=DragonWilds::NpcNetwork::DialogueFingerprint("base",graphs,otherIds));
    auto otherStory=graphs;otherStory["mod:story"]["Entry"]="different";
    assert(dialogueFingerprint!=DragonWilds::NpcNetwork::DialogueFingerprint("base",otherStory,manifest));
    auto otherQuest=manifest;otherQuest[0]["Definition"]["Id"]="different";
    assert(dialogueFingerprint!=DragonWilds::NpcNetwork::DialogueFingerprint("base",graphs,otherQuest));
    assert(Rejects([&]{DragonWilds::NpcNetwork::DialogueFingerprint("base",graphs,Json::array());}));
    auto multiplayerCow=cow;multiplayerCow["Multiplayer"]=true;
    NpcCatalog multiplayerCatalog;multiplayerCatalog.AddStore("Mod",shop);multiplayerCatalog.AddNpc("Mod",multiplayerCow);
    assert(multiplayerCatalog.Resolve()[0].Data["Multiplayer"]==true);
    multiplayerCow["Multiplayer"]="true";
    assert(Rejects([&]{NpcCatalog invalid;invalid.AddNpc("Mod",multiplayerCow);}));
    auto moved=cow;moved["Location"]={101,202,-303};
    moved["Rotation"]={{"Pitch",0},{"Yaw",45},{"Roll",0}};moved["Scale"]=1.5;
    NpcCatalog relocated;relocated.AddStore("Mod",shop);relocated.AddNpc("Mod",moved);
    const auto movedResult=relocated.Resolve();
    assert(movedResult[0].Data["Location"]==moved["Location"]);
    assert(movedResult[0].StoreOwner==result[0].StoreOwner);
    assert(DragonWilds::VendorIdentity::ForOwner(DragonWilds::VendorOffers::Owner("Mod",movedResult[0].Data["Id"]))
        ==DragonWilds::VendorIdentity::ForOwner(DragonWilds::VendorOffers::Owner("Mod",result[0].Data["Id"])));
    assert(DragonWilds::VendorIdentity::ForOwner(DragonWilds::VendorOffers::Owner("Mod",result[0].Data["Id"]))
        ==DragonWilds::VendorIdentity::ForOwner(DragonWilds::VendorOffers::Owner("Mod","cow")));
}
