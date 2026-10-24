#pragma once
#include <string>
#include <sstream>
#include <stdexcept>
namespace PS::ModIdentity {
struct Record {std::string Id,Name,Nickname,Author="Unknown Author",Website,Version,Build,NexusModID,NexusFileID,Description,Banner;bool Present=false;std::string DisplayName()const{return Nickname.empty()?Name:Nickname;}};
inline std::string Trim(std::string value){const auto first=value.find_first_not_of(" \t\r\n");return first==value.npos?std::string{}:value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);}
inline std::string NexusID(std::string value){
    auto lower=value;for(auto& ch:lower)if(ch>='A'&&ch<='Z')ch+=32;
    if(lower.empty()||lower=="unknown"||lower=="unset"||lower=="0")return {};
    if(value.size()>20||value.find_first_not_of("0123456789")!=value.npos||value.find_first_not_of('0')==value.npos)throw std::runtime_error("ID.txt Nexus IDs must be positive decimal IDs, blank, Unknown or unset.");return value;
}
inline bool RelativeBanner(const std::string& value){
    if(value.empty())return true;if(value.size()>1024||value.front()=='/'||value.front()=='\\'||value.find(':')!=value.npos||value.find('\\')!=value.npos)return false;
    std::istringstream parts(value);std::string part;while(std::getline(parts,part,'/'))if(part.empty()||part=="."||part=="..")return false;
    auto lower=value;for(auto& ch:lower)if(ch>='A'&&ch<='Z')ch+=32;return lower.ends_with(".png")||lower.ends_with(".jpg")||lower.ends_with(".jpeg")||lower.ends_with(".webp");
}
inline Record Parse(const std::string& contents,const std::string& folder){
    if(contents.size()>64*1024)throw std::runtime_error("ID.txt exceeds 64 KiB.");Record out;out.Id=folder;out.Name=folder;out.Present=true;
    auto text=contents;if(text.starts_with("\xef\xbb\xbf"))text.erase(0,3);std::istringstream input(text);std::string line;bool structured=false;
    while(std::getline(input,line)){line=Trim(line);if(line.empty()||line.starts_with('#')||line.starts_with(';'))continue;const auto split=line.find_first_of(":=");
        if(split==line.npos){if(!structured){out.Id=line;out.Name=line;}continue;}structured=true;const auto key=Trim(line.substr(0,split)),value=Trim(line.substr(split+1));
        if(key=="ModID"||key=="ModId")out.Id=value;else if(key=="Name"||key=="ModName")out.Name=value;
        else if(key=="Nickname"||key=="DisplayName")out.Nickname=value;else if(key=="Author"||key=="ModAuthor")out.Author=value;
        else if(key=="Website"||key=="ModWebsite"||key=="ModSite")out.Website=value;else if(key=="Version"||key=="ModVersion")out.Version=value;
        else if(key=="Build")out.Build=value;else if(key=="NexusModID")out.NexusModID=NexusID(value);else if(key=="NexusFileID")out.NexusFileID=NexusID(value);
        else if(key=="Description")out.Description=value;else if(key=="Banner")out.Banner=value;
    }
    if(out.Id.empty())throw std::runtime_error("ID.txt ModID cannot be empty.");if(out.Name.empty())out.Name=out.Id;
    if(out.Author.empty()||out.Id.size()>128||out.Name.size()>256||out.Nickname.size()>192||out.Author.size()>256||out.Website.size()>2048||out.Version.size()>64||out.Build.size()>64||out.Description.size()>1024||!RelativeBanner(out.Banner))throw std::runtime_error("ID.txt metadata exceeds its limits or has an unsafe banner path.");
    for(const auto* value:{&out.Id,&out.Name,&out.Nickname,&out.Author,&out.Website,&out.Version,&out.Build,&out.Description,&out.Banner})for(unsigned char c:*value)if(c<32||c==127)throw std::runtime_error("ID.txt metadata contains control characters.");
    if(!out.Website.empty()&&!out.Website.starts_with("https://")&&!out.Website.starts_with("http://"))throw std::runtime_error("ID.txt Website must use http or https.");return out;
}
}
