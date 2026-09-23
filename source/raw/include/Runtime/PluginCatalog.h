#pragma once
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace PS::PluginCatalog {
namespace fs = std::filesystem;
struct Plugin {
    std::string Id,Name,Version,BuiltForRuneSchema,ConsoleMessage;
    int ApiVersion=0;
    bool Enabled=true,Required=false;
    fs::path Root,DllRoot,PaksRoot,ScriptsRoot,EntryPoint;
    std::vector<std::string> Capabilities,Connections;
    std::vector<std::pair<std::string,std::string>> Dependencies;
};
struct OrderEntry { std::string Id;bool Enabled=true;size_t Position=0; };
inline std::vector<fs::path> PakDirectories(const Plugin& plugin) {
    std::vector<fs::path> result;std::error_code error;
    if(!fs::exists(plugin.PaksRoot,error))return result;
    if(!fs::is_directory(plugin.PaksRoot,error)||fs::is_symlink(fs::symlink_status(plugin.PaksRoot,error)))
        throw std::runtime_error("Plugin paks path is not a safe directory: "+plugin.Id);
    for(const auto& package:fs::directory_iterator(plugin.PaksRoot,fs::directory_options::skip_permission_denied,error)) {
        if(error)throw std::runtime_error("Unable to enumerate plugin paks: "+plugin.Id);
        if(package.is_symlink(error))throw std::runtime_error("Symlinks are not allowed under plugin paks: "+plugin.Id);
        if(!package.is_directory(error))throw std::runtime_error("Only named package directories are allowed directly under paks/: "+plugin.Id);
        std::map<fs::path,uint8_t> triplets;
        for(const auto& file:fs::directory_iterator(package.path(),fs::directory_options::skip_permission_denied,error)) {
            if(error)throw std::runtime_error("Unable to enumerate plugin package: "+plugin.Id);
            if(file.is_symlink(error)||!file.is_regular_file(error))throw std::runtime_error("Package directories may contain only regular container files: "+plugin.Id);
            auto extension=file.path().extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
            uint8_t bit=0;if(extension==L".pak")bit=1;else if(extension==L".ucas")bit=2;else if(extension==L".utoc")bit=4;else continue;
            auto base=file.path();base.replace_extension();triplets[base]|=bit;
        }
        if(triplets.empty()||std::any_of(triplets.begin(),triplets.end(),[](const auto& value){return value.second!=7;}))
            throw std::runtime_error("Incomplete pak/ucas/utoc triplet in "+package.path().string());
        result.push_back(package.path());
    }
    std::sort(result.begin(),result.end());return result;
}
inline bool Token(const std::string& value) {
    return !value.empty() && value.size()<=96 && std::all_of(value.begin(),value.end(),[](unsigned char c){
        return std::isalnum(c)||c=='.'||c=='_'||c=='-';
    });
}
inline std::vector<OrderEntry> ReadOrder(const fs::path& root) {
    const auto path=root/"plugins.txt";std::vector<OrderEntry> result;if(!fs::is_regular_file(path))return result;
    if(fs::file_size(path)>256*1024)throw std::runtime_error("plugins.txt exceeds 256 KiB");
    std::ifstream stream(path);std::string line;size_t position=0;std::unordered_set<std::string> seen;
    while(std::getline(stream,line)) {
        const auto first=line.find_first_not_of(" \t\r\n");if(first==std::string::npos||line[first]=='#'||line[first]==';')continue;
        const auto colon=line.find(':',first);if(colon==std::string::npos)throw std::runtime_error("Invalid plugins.txt entry: "+line);
        auto id=line.substr(first,colon-first),value=line.substr(colon+1);
        const auto idLast=id.find_last_not_of(" \t\r\n");id.resize(idLast==std::string::npos?0:idLast+1);
        const auto valueFirst=value.find_first_not_of(" \t\r\n"),valueLast=value.find_last_not_of(" \t\r\n");
        value=valueFirst==std::string::npos?std::string{}:value.substr(valueFirst,valueLast-valueFirst+1);
        if(!Token(id)||(value!="0"&&value!="1")||!seen.emplace(id).second)throw std::runtime_error("Invalid or duplicate plugins.txt entry: "+id);
        result.push_back({std::move(id),value=="1",position++});
    }
    return result;
}
inline std::vector<Plugin> Discover(const fs::path& root,std::vector<std::string>* diagnostics=nullptr) {
    std::vector<Plugin> result;if(!fs::is_directory(root))return result;
    std::unordered_set<std::string> ids;
    for(const auto& folder:fs::directory_iterator(root)) {
        if(!folder.is_directory()||folder.is_symlink())continue;
        const auto manifest=folder.path()/"plugin.json";if(!fs::is_regular_file(manifest))continue;
        try {
        if(fs::file_size(manifest)>256*1024)throw std::runtime_error("Plugin manifest exceeds 256 KiB: "+manifest.string());
        std::ifstream stream(manifest);nlohmann::json data;stream>>data;
        for(const auto* field:{"SchemaVersion","Id","Version"})
            if(!data.contains(field))throw std::runtime_error(std::string("Plugin manifest missing ")+field+": "+manifest.string());
        if(data.at("SchemaVersion")!=1)throw std::runtime_error("Unsupported plugin manifest schema: "+manifest.string());
        Plugin plugin;plugin.Id=data.at("Id").get<std::string>();plugin.Name=data.value("Name",plugin.Id);
        plugin.Version=data.at("Version").get<std::string>();plugin.ApiVersion=data.value("ApiVersion",1);
        plugin.BuiltForRuneSchema=data.value("BuiltForRuneSchema",std::string{});
        plugin.ConsoleMessage=data.value("ConsoleMessage",std::string{});
        plugin.Enabled=data.value("Enabled",true);plugin.Required=data.value("Required",false);plugin.Root=folder.path();
        if(!Token(plugin.Id)||!Token(plugin.Version)||(!plugin.BuiltForRuneSchema.empty()&&!Token(plugin.BuiltForRuneSchema))
            ||plugin.Name.empty()||plugin.Name.size()>128||plugin.ConsoleMessage.size()>512
            ||plugin.ConsoleMessage.find_first_of("\r\n")!=std::string::npos||!ids.emplace(plugin.Id).second)
            throw std::runtime_error("Invalid or duplicate plugin identity/wording: "+plugin.Id);
        if(data.contains("Capabilities")) {
            if(!data.at("Capabilities").is_array()||data.at("Capabilities").size()>64)throw std::runtime_error("Invalid plugin capabilities: "+plugin.Id);
            for(const auto& capability:data.at("Capabilities")){auto value=capability.get<std::string>();if(!Token(value))throw std::runtime_error("Invalid plugin capability: "+plugin.Id);plugin.Capabilities.push_back(std::move(value));}
        }
        if(data.contains("Connections")) {
            if(!data.at("Connections").is_array()||data.at("Connections").size()>32)throw std::runtime_error("Invalid plugin connections: "+plugin.Id);
            for(const auto& connection:data.at("Connections")){auto value=connection.get<std::string>();if(!Token(value))throw std::runtime_error("Invalid plugin connection: "+plugin.Id);plugin.Connections.push_back(std::move(value));}
        }
        if(data.contains("Dependencies")) {
            if(!data.at("Dependencies").is_object()||data.at("Dependencies").size()>32)throw std::runtime_error("Invalid plugin dependencies: "+plugin.Id);
            for(const auto& [id,version]:data.at("Dependencies").items()) {
                if(!Token(id)||!version.is_string()||version.get_ref<const std::string&>().size()>32)throw std::runtime_error("Invalid plugin dependency: "+plugin.Id);
                plugin.Dependencies.emplace_back(id,version.get<std::string>());
            }
        }
        if(data.contains("PackagedRoot"))throw std::runtime_error("PackagedRoot is obsolete; use the fixed paks/<PackageName> layout: "+plugin.Id);
        plugin.DllRoot=plugin.Root/"dll";plugin.PaksRoot=plugin.Root/"paks";plugin.ScriptsRoot=plugin.Root/"scripts";
        if(data.contains("EntryPoint")) {
            const auto entry=fs::path(data.at("EntryPoint").get<std::string>());
            if(entry.is_absolute()||entry.empty()||entry.has_parent_path()||entry.extension()!=L".dll")throw std::runtime_error("Plugin EntryPoint must be a DLL filename under dll/: "+plugin.Id);
            // A missing DLL does not hide the manifest or its PAKs.
            plugin.EntryPoint=plugin.DllRoot/entry;
        }
        result.push_back(std::move(plugin));
        } catch(const std::exception& error) {
            if(diagnostics)diagnostics->push_back(folder.path().filename().string()+": rejected: "+error.what());
        } catch(...) {
            if(diagnostics)diagnostics->push_back(folder.path().filename().string()+": rejected by an unknown manifest error");
        }
    }
    const auto requested=ReadOrder(root);std::unordered_map<std::string,OrderEntry> requestedById;
    for(const auto& entry:requested)requestedById.emplace(entry.Id,entry);
    for(auto& plugin:result)if(const auto found=requestedById.find(plugin.Id);found!=requestedById.end()) {
        if(plugin.Required&&!found->second.Enabled&&diagnostics)
            diagnostics->push_back(plugin.Id+": legacy Required flag ignored; explicit plugins.txt disable wins and RuneSchema core remains independent");
        plugin.Enabled=plugin.Enabled&&found->second.Enabled;
    }
    std::sort(result.begin(),result.end(),[&](const auto& a,const auto& b){
        const auto left=requestedById.find(a.Id),right=requestedById.find(b.Id);
        if(left!=requestedById.end()&&right!=requestedById.end())return left->second.Position<right->second.Position;
        if(left!=requestedById.end())return true;if(right!=requestedById.end())return false;return a.Id<b.Id;
    });
    std::unordered_map<std::string,size_t> byId;for(size_t i=0;i<result.size();++i)byId.emplace(result[i].Id,i);
    std::vector<bool> valid(result.size());for(size_t i=0;i<result.size();++i)valid[i]=result[i].Enabled;
    for(size_t i=0;i<result.size();++i)if(valid[i])for(const auto& [dependency,declaredVersion]:result[i].Dependencies){
        const auto found=byId.find(dependency);
        if((found==byId.end()||!valid[found->second])&&diagnostics)
            diagnostics->push_back(result[i].Id+": dependency "+dependency+" is unavailable; loading without it");
    }
    std::vector<size_t> indegree(result.size());for(size_t i=0;i<result.size();++i)if(valid[i])for(const auto& dependency:result[i].Dependencies){const auto found=byId.find(dependency.first);if(found!=byId.end()&&valid[found->second])++indegree[i];}
    std::vector<Plugin> ordered;ordered.reserve(result.size());std::vector<bool> emitted(result.size());
    bool progress=true;while(progress){progress=false;for(size_t i=0;i<result.size();++i)if(valid[i]&&!emitted[i]&&indegree[i]==0){emitted[i]=true;progress=true;ordered.push_back(result[i]);for(size_t consumer=0;consumer<result.size();++consumer)if(valid[consumer]&&!emitted[consumer])for(const auto& dependency:result[consumer].Dependencies)if(dependency.first==result[i].Id&&indegree[consumer])--indegree[consumer];}}
    for(size_t i=0;i<result.size();++i)if(valid[i]&&!emitted[i]) {
        if(diagnostics)diagnostics->push_back(result[i].Id+": dependency cycle detected; using manifest order");
        ordered.push_back(result[i]);
    }
    return ordered;
}
}
