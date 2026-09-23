#pragma once
#include "Loader/DialogueProgress.h"
#include "Loader/QuestNativeAdapter.h"
#include "Loader/DialogueSaveIdentity.h"
namespace DragonWilds::DialogueSave {
class State {
    QuestNative::Adapter native;
    std::string mod;
    int Read(const std::string& key)const{return native.GetInt(RC::Unreal::FName(RC::to_generic_string(key).c_str(),RC::Unreal::FNAME_Add));}
    void Write(const std::string& key,int value)const{native.SetInt(RC::Unreal::FName(RC::to_generic_string(key).c_str(),RC::Unreal::FNAME_Add),value);}
    std::string FlagKey(const std::string& flag)const {
        if(!flag.starts_with(mod+":"))throw std::runtime_error("Dialogue flag owner mismatch");
        Dialogue::Reference("_",flag);return "RuneSchema.Flag:"+flag;
    }
public:
    State(RC::Unreal::UObject* controller,RC::Unreal::UObject* asset,const std::string& owner,
        const std::filesystem::path& legacy,const std::string& character):native(controller,asset),mod(owner) {
        native.ValidateCounters();if(!native.IsInitialized())native.Initialize();
        const auto version=Read("RuneSchema.DialogueVersion");
        if(!version) {
            const auto old=DialogueProgress::Read(legacy,character);
            for(const auto& [flag,status]:old.at("Flags").items())if(flag.starts_with(mod+":"))Write(FlagKey(flag),status=="complete"?2:1);
            for(const auto& variable:Quests::OwnershipVariables(mod,PersistenceId(mod)))Write(variable.at("QuestVariableName").get<std::string>(),Quests::OwnershipVersion);
            Write("RuneSchema.DialogueVersion",Quests::OwnershipVersion);
        } else if(version!=Quests::OwnershipVersion)throw std::runtime_error("Unsupported saved dialogue version");
    }
    bool HasFlag(const std::string& flag)const {
        const auto value=Read(FlagKey(flag));if(value<0 || value>2)throw std::runtime_error("Invalid saved dialogue flag");return value==2;
    }
    template<class Ready,class Give> DialogueProgress::Result Complete(const std::string& flag,Ready ready,Give give)const {
        using R=DialogueProgress::Result;
        const auto key=FlagKey(flag);
        const auto phase=Read(key);
        if(phase==2)return R::AlreadyComplete;
        if(phase==1)return R::Uncertain;
        if(phase!=0)throw std::runtime_error("Invalid saved dialogue flag");
        if(!ready())return R::NotReady;
        Write(key,1);
        if(!give())return R::Uncertain;
        Write(key,2);return R::Granted;
    }
};
}
