#pragma once
#include "Core/ConfigFiles.h"
#include "Generator/SaveReport.h"
#include "Loader/DialogueDefinition.h"

namespace DragonWilds::DialogueProgress {
using nlohmann::json;
enum class Result { Granted, AlreadyComplete, NotReady, Uncertain };
inline json Read(const std::filesystem::path& path,const std::string& character) {
    if(PS::SaveReport::Guid(character)!=character)throw std::runtime_error("Dialogue requires a stable character GUID");
    if(!std::filesystem::exists(path))return {{"Version",1},{"CharacterGuid",character},{"Flags",json::object()}};
    auto data=json::parse(PS::ConfigFiles::Read(path,256*1024));
    Dialogue::Fields(data,{"Version","CharacterGuid","Flags"});
    if(data.at("Version")!=1 || data.at("CharacterGuid")!=character || !data.at("Flags").is_object() || data["Flags"].size()>512)
        throw std::runtime_error("Unsupported or mismatched dialogue progress file; original preserved");
    for(const auto& [key,value]:data["Flags"].items()) {
        Dialogue::Reference("_",key);
        if(!value.is_string() || (value!="complete" && value!="pending"))
            throw std::runtime_error("Invalid dialogue progress state; original preserved");
    }
    return data;
}
inline bool HasFlag(const std::filesystem::path& path,const std::string& character,const std::string& flag) {
    const auto data=Read(path,character);
    return data.at("Flags").value(flag,std::string{})=="complete";
}
template<class Ready,class Give>
Result Complete(const std::filesystem::path& path,const std::string& character,const std::string& flag,Ready ready,Give give) {
    auto data=Read(path,character);
    Dialogue::Reference("_",flag);
    const auto status=data.at("Flags").value(flag,std::string{});
    if(status=="complete")return Result::AlreadyComplete;
    if(status=="pending")return Result::Uncertain;
    if(data["Flags"].size()>=512)throw std::runtime_error("Dialogue progress flag limit reached");
    if(!ready())return Result::NotReady;
    data["Flags"][flag]="pending";
    PS::ConfigFiles::Write(path,data.dump(2));
    // A crash/exception after reservation must not automatically repeat an uncertain grant.
    if(!give()) {
        data["Flags"].erase(flag);
        PS::ConfigFiles::Write(path,data.dump(2));
        return Result::NotReady;
    }
    data["Flags"][flag]="complete";
    PS::ConfigFiles::Write(path,data.dump(2));
    return Result::Granted;
}
}
