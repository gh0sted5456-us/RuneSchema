#pragma once
#include "Loader/QuestReceiptJournal.h"
#include "Loader/QuestNativeAdapter.h"
namespace DragonWilds::Quests {
class NativeReceipt : public ReceiptJournal {
    static RC::Unreal::FName Name(const std::string& key) {
        return RC::Unreal::FName(RC::to_generic_string(key).c_str(),RC::Unreal::FNAME_Add);
    }
public:
    NativeReceipt(const QuestNative::Adapter& api,const Definition& definition,const Json& document,
        const std::filesystem::path& legacy,const std::string& character):ReceiptJournal(definition,document,legacy,character,
            [&api](const std::string& key){return api.GetInt(Name(key));},
            [&api](const std::string& key,int value){api.SetInt(Name(key),value);},
            [&api]{api.ValidateCounters();return api.IsInitialized();},[&api]{api.Initialize();},[&api]{return RC::to_string(api.StateName());},
            ReceiptJournal::EpochNow,[&api]{api.ResetForDefinitionChange();}) {}
};
}
