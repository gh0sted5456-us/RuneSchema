#include "Core/ModLicenses.h"
#include <iostream>
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Mod license regression");}
int main(int argc,char** argv)try{
    if(argc==4&&std::string_view(argv[1])=="--reject-linked-folder"){
        bool refused=false;try{(void)PS::ModLicenses::Read(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]));}catch(...){refused=true;}
        Check(refused);std::cout<<"Linked mod folder refused.\n";return 0;
    }
    namespace fs=std::filesystem;using namespace PS;
    for(const auto* name:{"LICENSE","license.txt","LiCeNcE.MD","LICENSE-MIT","LICENSE-APACHE.txt","license-jonesing4space.txt","COPYING","COPYING.LESSER","NOTICE.txt","conditions.license","Third-Party-Notices.txt"})Check(ModLicenses::Filename(name));
    for(const auto* name:{"license.dll","license.png","main.lua","README.md","licensedata.txt","conditions.exe"})Check(!ModLicenses::Filename(name));
    const auto base=fs::temp_directory_path()/("mod-licenses-"+std::to_string(GetCurrentProcessId()));const auto root=base/"ue4ss/Mods/RuneSchema",folder=root/"mods/Example";
    struct Cleanup{fs::path Path;~Cleanup(){std::error_code error;fs::remove_all(Path,error);}}cleanup{base};
    ConfigFiles::Write(folder/"LICENSE","Original terms\n100% unchanged.");ConfigFiles::Write(folder/"conditions.license","Additional conditions");
    ConfigFiles::Write(folder/"licenses/MIT.txt","MIT terms");ConfigFiles::Write(folder/"legal/LICENSE-APACHE.txt","Apache terms");
    ConfigFiles::Write(folder/"cache/LICENSE","Unrelated cached content");ConfigFiles::Write(folder/"nested/LICENSE","Unrelated loader");
    auto result=ModLicenses::Read(root,folder);Check(result.Notices.size()==4&&result.Issues.empty());
    bool exact=false;for(const auto& notice:result.Notices)exact|=notice.Name=="LICENSE"&&notice.Text=="Original terms\n100% unchanged.";Check(exact);
    ConfigFiles::Write(folder/"COPYING.txt",std::string(256*1024+1,'x'));result=ModLicenses::Read(root,folder);Check(result.Notices.size()==4&&result.Issues.size()==1);
    fs::create_directories(root.parent_path()/"External");ConfigFiles::Write(root.parent_path()/"External/LICENCE.md","External terms");Check(ModLicenses::Read(root,root.parent_path()/"External").Notices.size()==1);
    ConfigFiles::Write(folder/"ue4ss/Loader/LICENSE","Loader terms");Check(ModLicenses::Read(root,folder/"ue4ss/Loader").Notices.size()==1);
    bool refused=false;try{(void)ModLicenses::Read(root,base);}catch(...){refused=true;}Check(refused);
    refused=false;try{(void)ModLicenses::Read(root,root);}catch(...){refused=true;}Check(refused);
    Check(ModLicenses::Text("\xef\xbb\xbfMIT")=="MIT");Check(ModLicenses::Text(std::string("\xff\xfeM\0I\0T\0",8))=="MIT");
    Check(ModLicenses::Text(std::string("\xfe\xff\0M\0I\0T",8))=="MIT");Check(ModLicenses::Text("Copyright \xa9")=="Copyright \xc2\xa9");
    refused=false;try{(void)ModLicenses::Text(std::string("binary\0text",11));}catch(...){refused=true;}Check(refused);
    const auto link=folder/"NOTICE.txt";if(CreateSymbolicLinkW(link.c_str(),(root.parent_path()/"External/LICENCE.md").c_str(),SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)){
        result=ModLicenses::Read(root,folder);Check(result.Notices.size()==4&&result.Issues.size()==2);
    }else std::cout<<"File-symlink fixture unavailable on this Windows account; use --reject-linked-folder for the junction fixture.\n";
    std::cout<<"License discovery, scope, original text, encodings, file bounds passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
