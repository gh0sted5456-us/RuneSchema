#include "Core/ItemAuthoringDraft.h"
#include <cassert>
#include <memory>
#include <set>
using namespace PS::ItemAuthoring;
template<class F> void rejects(F&& f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
// Independent three-byte-group codec: check all 128 payload bits, not merely
// whether generated text resembles a PersistenceID.
static std::array<unsigned char,16> DecodeIdentity(const std::string& text){
    constexpr std::string_view alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    assert(text.size()==22);std::array<unsigned char,16> bytes{};size_t at=0;unsigned value=0;int bits=0;
    for(char c:text){const auto index=alphabet.find(c);assert(index!=alphabet.npos);value=(value<<6)|unsigned(index);bits+=6;
        if(bits>=8){bits-=8;assert(at<bytes.size());bytes[at++]=static_cast<unsigned char>(value>>bits);}}
    assert(at==16&&bits==4&&(value&15)==0);return bytes;
}
static std::string EncodeIdentity(const std::array<unsigned char,16>& bytes){
    constexpr std::string_view alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";std::string text;
    for(size_t at=0;at<bytes.size();at+=3){const auto remaining=bytes.size()-at;const unsigned word=(unsigned(bytes[at])<<16)|(remaining>1?unsigned(bytes[at+1])<<8:0)|(remaining>2?unsigned(bytes[at+2]):0);
        text+=alphabet[(word>>18)&63];text+=alphabet[(word>>12)&63];if(remaining>1)text+=alphabet[(word>>6)&63];if(remaining>2)text+=alphabet[word&63];}return text;
}
int main(){
    const auto sequence=DecodeIdentity("AAECAwQFBgcICQoLDA0ODw");for(size_t i=0;i<sequence.size();++i)assert(sequence[i]==i);
    assert(EncodeIdentity(sequence)=="AAECAwQFBgcICQoLDA0ODw");
    const auto allBits=DecodeIdentity("_____________________w");for(auto byte:allBits)assert(byte==255);
    std::set<std::string> generated;
    for(int i=0;i<2048;++i){const auto id=PS::ClonePresentation::NewId("Mod");assert(PS::ClonePresentation::ValidId(id,"DifferentMod"));assert(EncodeIdentity(DecodeIdentity(id))==id);assert(generated.insert(id).second);const auto name=PS::ClonePresentation::InternalName(id);assert(name.find('-')==name.npos);}
    assert(PS::ClonePresentation::ValidId("xBicTfzWYkeOCtOLKtSRFQ","AnyMod"));
    assert(!PS::ClonePresentation::ValidId("xBicTfzWYkeOCtOLKtSRF1","AnyMod"));
    assert(!PS::ClonePresentation::ValidId("xBicTfzWYkeOCtOLKtSRF!","AnyMod"));
    assert(PS::ClonePresentation::InternalName("a-b_c")!=PS::ClonePresentation::InternalName("a_b-c"));
    auto owner=std::make_unique<Draft>();auto& d=*owner;
    d.SetSource({{"Path","/Game/Items/Sword.Sword"},{"Name","Sword"}});assert(d.InspectWanted&&!d.Ready);
    d.Inspection={{"Fields",Json::array({{{"Name","PowerLevel"},{"Editable",true}},{{"Name","@row/WeaponStats/DamageMultiplier"},{"Editable",true}},{{"Name","PersistenceID"},{"Editable",false}}})}};d.Ready=true;
    d.Appearance={{"Path","/Game/Items/Axe.Axe"}};
    std::snprintf(d.Editor.data(),d.Editor.size(),"%s",R"({ // source values
      "PowerLevel": 10, "@row/WeaponStats/DamageMultiplier": 3.5
    })");
    const auto creation=Request(d,Mode::Creation,"",true);assert(creation["Action"]=="PrepareClone"&&creation["Give"]==false&&creation["Permanent"]==true&&!creation.contains("Player"));
    assert(creation["Overrides"]["PowerLevel"]==10&&creation["StatOverrides"]["WeaponStats"]["DamageMultiplier"]==3.5&&creation["AppearanceSource"]=="");
    const auto transmog=Request(d,Mode::Transmog,"",true);assert(transmog["Source"]=="/Game/Items/Sword.Sword"&&transmog["AppearanceSource"]=="/Game/Items/Axe.Axe"&&transmog["Overrides"].empty()&&transmog["StatOverrides"].empty());
    d.Source["Icon"]="/Game/UI/Sword.Sword";d.Appearance["Icon"]="/Mod/UI/Axe.Axe";
    assert(Preview(d,Mode::Transmog)["Icon"]=="/Mod/UI/Axe.Axe");
    d.Result={{"State","Prepared"},{"Icon","/Mod/UI/NativeSkin.NativeSkin"}};
    assert(Preview(d,Mode::Transmog)["Icon"]=="/Mod/UI/NativeSkin.NativeSkin");
    d.Result={{"State","Created"},{"Icon","/Mod/UI/NativeSkin.NativeSkin"},{"Path","/Game/RuneSchema/Items/Clone.Clone"}};d.Created=true;
    assert(Preview(d,Mode::Transmog)["Path"]=="/Game/RuneSchema/Items/Clone.Clone");
    d.Changed();assert(d.Result.empty()&&Preview(d,Mode::Transmog)["Icon"]=="/Mod/UI/Axe.Axe");
    d.Appearance["Icon"]="";assert(Preview(d,Mode::Transmog)["Icon"]=="/Game/UI/Sword.Sword");
    rejects([&]{Request(d,Mode::Creation,"player",false);});d.Acknowledged=true;assert(Request(d,Mode::Creation,"player",false)["Action"]=="CreateClone");
    Json direct,stats;rejects([&]{SplitValues({{"PersistenceID","bad"}},d.Inspection,direct,stats);});rejects([&]{SplitValues({{"Unknown",1}},d.Inspection,direct,stats);});rejects([]{ParseValues("[]");});rejects([]{ParseValues("{bad}");});
    const auto id=d.Id;d.Created=true;d.Changed();assert(d.Id.empty()&&!d.Created&&!d.Acknowledged);
    auto other=std::make_unique<Draft>();other->SetSource({{"Path","/Game/Items/Ore.Ore"}});assert(d.Source["Path"]=="/Game/Items/Sword.Sword"&&d.Appearance["Path"]=="/Game/Items/Axe.Axe");
    const auto root=std::filesystem::temp_directory_path()/("helpy-authoring-"+std::to_string(GetCurrentProcessId()));
    const auto internal=PS::ClonePresentation::InternalName(id);const auto target="/Game/Items/"+internal+"."+internal;
    Json prepared={{"State","Prepared"},{"InternalName",internal}};
    prepared["Documents"]["assets"][target]={{"$Clone","/Game/Items/Sword.Sword"},{"PersistenceID",id}};
    prepared["Documents"]["raw"]["/Game/Stats.Table"][id]={{"DamageMultiplier",3.5}};
    const auto first=Export(root,prepared),second=Export(root,prepared);assert(first!=second);
    assert(Json::parse(PS::ConfigFiles::Read(first/"assets"/(internal+".jsonc")),nullptr,true,true)==prepared["Documents"]["assets"]);
    assert(Json::parse(PS::ConfigFiles::Read(first/"raw"/(internal+".jsonc")),nullptr,true,true)==prepared["Documents"]["raw"]);
    auto bad=prepared;bad["InternalName"]="../escape";rejects([&]{Export(root,bad);});
    std::filesystem::remove_all(root);
}
