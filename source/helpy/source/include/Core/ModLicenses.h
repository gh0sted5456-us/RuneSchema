#pragma once
#include "Core/ModManager.h"
#include <vector>

namespace PS::ModLicenses {
namespace fs=std::filesystem;
struct Notice {std::string Name,Text;};
struct Result {std::vector<Notice> Notices;std::vector<std::string> Issues;};
inline bool Filename(std::string name){
    name=ModManager::Order::Fold(name);
    if(name.ends_with(".license")||name.ends_with(".licence"))return true;
    const auto extension=fs::path(name).extension().string();
    if(!extension.empty()&&extension!=".txt"&&extension!=".md"&&extension!=".rst"&&extension!=".markdown"&&extension!=".lesser"&&extension!=".gpl"&&extension!=".lgpl"&&extension!=".mit"&&extension!=".bsd")return false;
    for(const std::string_view prefix:{"license","licence","copying","copyright","unlicense","notice","third-party-notices","third_party_notices","terms","conditions"})
        if(name==prefix||(name.starts_with(prefix)&&name.size()>prefix.size()&&std::string_view("._- ").find(name[prefix.size()])!=std::string_view::npos))return true;
    return false;
}
inline std::string Text(std::string bytes){
    if(bytes.starts_with("\xef\xbb\xbf"))bytes.erase(0,3);
    else if(bytes.starts_with("\xff\xfe")||bytes.starts_with("\xfe\xff")){
        const bool little=static_cast<unsigned char>(bytes[0])==255;
        if(bytes.size()%2)throw std::runtime_error("Incomplete UTF-16 license text");
        std::wstring wide;wide.reserve((bytes.size()-2)/2);
        for(size_t i=2;i<bytes.size();i+=2){const auto a=static_cast<unsigned char>(bytes[i]),b=static_cast<unsigned char>(bytes[i+1]);wide+=wchar_t(little?a|(b<<8):b|(a<<8));}
        if(wide.empty())return {};
        const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),int(wide.size()),nullptr,0,nullptr,nullptr);
        if(!size)throw std::runtime_error("Invalid UTF-16 license text");
        bytes.resize(size);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),int(wide.size()),bytes.data(),size,nullptr,nullptr);
    }
    if(std::any_of(bytes.begin(),bytes.end(),[](unsigned char c){return c<32&&c!='\r'&&c!='\n'&&c!='\t';}))throw std::runtime_error("License file is not plain text");
    if(!bytes.empty()&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),nullptr,0)){
        const auto count=MultiByteToWideChar(1252,0,bytes.data(),int(bytes.size()),nullptr,0);std::wstring wide(count,0);
        if(!count||!MultiByteToWideChar(1252,0,bytes.data(),int(bytes.size()),wide.data(),count))throw std::runtime_error("Cannot decode license text");
        const auto size=WideCharToMultiByte(CP_UTF8,0,wide.data(),count,nullptr,0,nullptr,nullptr);bytes.resize(size);
        if(!size||!WideCharToMultiByte(CP_UTF8,0,wide.data(),count,bytes.data(),size,nullptr,nullptr))throw std::runtime_error("Cannot decode license text");
    }
    return bytes;
}
inline Result Read(const fs::path& assignedRoot,const fs::path& selectedFolder){
    const auto root=fs::absolute(assignedRoot).lexically_normal(),folder=fs::absolute(selectedFolder).lexically_normal();
    ModManager::Safe(root);ModManager::Safe(folder);
    const auto relative=folder.lexically_relative(root.parent_path());
    if(relative.empty()||relative.is_absolute())throw std::runtime_error("The selected mod is outside this runtime");
    for(const auto& part:relative)if(part==".."||part=="."||part.string().find(':')!=std::string::npos)throw std::runtime_error("The selected mod is outside this runtime");
    auto part=relative.begin();
    if(ModManager::Order::Fold(part->string())==ModManager::Order::Fold(root.filename().string())){
        if(++part==relative.end()||ModManager::Order::Fold(part->string())!="mods"||++part==relative.end())throw std::runtime_error("Select an installed mod, not RuneSchema itself");
    }else if(++part!=relative.end())throw std::runtime_error("Select an installed UE4SS mod folder");
    if(!fs::is_directory(folder))throw std::runtime_error("This installed mod folder is unavailable");
    Result result;std::vector<fs::path> files,directories{folder};size_t scanned=0;
    const auto scan=[&](const fs::path& directory,bool subfolders){
        for(const auto& entry:fs::directory_iterator(directory)){
            if(++scanned>4096)throw std::runtime_error("License discovery reached its folder-entry limit");
            const auto attributes=GetFileAttributesW(entry.path().c_str());if(attributes==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Could not inspect a license input");
            const auto name=ModManager::Order::Fold(entry.path().filename().string());
            if(attributes&FILE_ATTRIBUTE_REPARSE_POINT){if(Filename(name))result.Issues.push_back(name+": linked license files are not read");continue;}
            const auto extension=ModManager::Order::Fold(entry.path().extension().string());
            const bool legalText=!subfolders&&(extension.empty()||extension==".txt"||extension==".md"||extension==".rst"||extension==".markdown");
            if(entry.is_regular_file()&&(Filename(name)||legalText)){if(files.size()>=32)throw std::runtime_error("License discovery reached its 32-file limit");files.push_back(entry.path());}
            else if(subfolders&&entry.is_directory()&&(name=="licenses"||name=="licences"||name=="legal"))directories.push_back(entry.path());
        }
    };
    scan(folder,true);for(size_t i=1;i<directories.size();++i)scan(directories[i],false);
    std::sort(files.begin(),files.end(),[&](const auto& a,const auto& b){return ModManager::Order::Fold(a.lexically_relative(folder).generic_string())<ModManager::Order::Fold(b.lexically_relative(folder).generic_string());});
    size_t total=0;
    for(const auto& file:files){const auto name=file.lexically_relative(folder).generic_string();try{
        ModManager::Safe(file);auto bytes=ConfigFiles::Read(file,256*1024);total+=bytes.size();if(total>2*1024*1024){result.Issues.push_back("Additional notices exceed the 2 MiB reading limit; view them in the installed folder.");break;}
        auto text=Text(std::move(bytes));if(!text.empty())result.Notices.push_back({name,std::move(text)});
    }catch(const std::exception& error){result.Issues.push_back(name+": "+error.what());}}
    return result;
}
}
