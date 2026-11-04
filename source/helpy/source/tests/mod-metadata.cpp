#include "Core/HelpyModMetadata.h"
#include "Core/ModDeployment.h"
#include <stdexcept>
static void Check(bool ok){if(!ok)throw std::runtime_error("Mod metadata regression");}
int main(){
    namespace M=PS::HelpyModMetadata;namespace fs=std::filesystem;using Json=nlohmann::json;
    const auto root=fs::temp_directory_path()/("mod-metadata-"+std::to_string(GetCurrentProcessId()));const auto data=root/"AppData",folder=root/"mods/Example";
    fs::create_directories(folder/"paks");PS::ConfigFiles::Write(folder/"paks/source.pak","fixture v1");
    try{
        const auto tips=PS::ModIdentity::Parse("ModID=Example\nVersion=1\n[Tips]\nTip=First tip\n2=Second tip\n- Third tip\n[Metadata]\nAuthor=Author name\n","Folder");Check(tips.Id=="Example"&&tips.Version=="1"&&tips.Author=="Author name"&&tips.Tips==std::vector<std::string>{"First tip","Second tip","Third tip"});
        bool badTip=false;try{PS::ModIdentity::Parse("ModID=Example\n[Tips]\nTip="+std::string(513,'x'),"Folder");}catch(...){badTip=true;}Check(badTip);
        const auto before=M::Read(root,"Example",data);Check(!before["Exists"].get<bool>()&&before["Track"].get<bool>());
        const std::string text="; retained comment\nModID=Example\nName=Example pack\nVersion=\nAuthor=Test\nCustomField=kept\n";
        M::Save(root,"Example",data,before,text,true);Check(PS::ConfigFiles::Read(folder/"ID.txt")==text&&fs::is_regular_file(folder/".helpy-source"));
        Json snapshot={{"Root",root.string()},{"Rows",Json::array({{{"Name","Example"},{"Present",true},{"Folder",folder.string()}}})}};
        M::UpdateTracking(snapshot,data);const auto first=snapshot["Rows"][0]["LocalFingerprint"].get<std::string>();Check(PS::CacheBundleIO::Digest(first));
        fs::create_directories(folder/"cache");PS::ConfigFiles::Write(folder/"cache/ignored.json","new cache");Check(M::Fingerprint(folder)==first);
        PS::ConfigFiles::Write(folder/"cache/tips.json","{\"Format\":99,\"Tips\":[\"Use the workbench\"]}");Check(PS::ModManager::Metadata(folder)["tips"][0]=="Use the workbench");
        auto cachedTips=PS::CacheTips::Read(folder/"cache/tips.json","mods/example");Json enabled=Json::array({{{"Name","Example"},{"Enabled",true},{"Present",true},{"Metadata",Json::object()}},{{"Name","Example"},{"Enabled",false},{"Present",true},{"Metadata",Json::object()}}});PS::CacheTips::Annotate(enabled,cachedTips);Check(enabled[0]["Metadata"]["tips"].size()==1&&!enabled[1]["Metadata"].contains("tips"));
        PS::ConfigFiles::Write(folder/"cache/tips.json","{\"Tips\":[{\"Owner\":\"mods/other\",\"Text\":\"Wrong owner\"}]}");Check(PS::ModManager::Metadata(folder).contains("tipsIssue"));
        const auto stamp=fs::last_write_time(folder/"paks/source.pak");PS::ConfigFiles::Write(folder/"paks/source.pak","fixture v2");fs::last_write_time(folder/"paks/source.pak",stamp);
        M::UpdateTracking(snapshot,data);Check(snapshot["Rows"][0]["LocalFingerprint"]!=first);Check(PS::ConfigFiles::Read(folder/"ID.txt")==text);
        bool stale=false;try{M::Save(root,"Example",data,before,text,true);}catch(...){stale=true;}Check(stale&&PS::ConfigFiles::Read(folder/"ID.txt")==text);
        auto expected=M::Read(root,"Example",data);bool invalid=false;try{M::Save(root,"Example",data,expected,"ModID=\n",true);}catch(...){invalid=true;}Check(invalid&&PS::ConfigFiles::Read(folder/"ID.txt")==text);
        M::Save(root,"Example",data,expected,text,false);Check(!M::Read(root,"Example",data)["Track"].get<bool>());snapshot["Rows"][0].erase("LocalFingerprint");M::UpdateTracking(snapshot,data);Check(!snapshot["Rows"][0].contains("LocalFingerprint"));
        M::Save(root,"Example",data,M::Read(root,"Example",data),text,true);PS::ConfigFiles::Write(folder/"ID.txt","ModID=Example\nVersion=2.0\n");snapshot["Rows"][0].erase("LocalFingerprint");M::UpdateTracking(snapshot,data);Check(!snapshot["Rows"][0].contains("LocalFingerprint"));
        bool traversal=false;try{M::Folder(root,"../Example");}catch(...){traversal=true;}Check(traversal);
        Json manifest={{"version",99},{"files",Json::array({{{"relPath","mods/Example/paks/source.pak"},{"source","example-download"}}})}};
        Check(PS::ModDeployment::VortexOwns(manifest,root,folder));manifest["files"][0]["relPath"]="../Example/paks/source.pak";Check(!PS::ModDeployment::VortexOwns(manifest,root,folder));
        manifest["files"][0]["relPath"]="mods/Other/paks/source.pak";Check(!PS::ModDeployment::VortexOwns(manifest,root,folder));
    }catch(...){fs::remove_all(root);throw;}fs::remove_all(root);
}
