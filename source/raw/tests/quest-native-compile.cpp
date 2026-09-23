#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/NameTypes.hpp"
#include "Loader/QuestNativeAdapter.h"
#include "Loader/QuestNativeRegistry.h"
#include "Loader/QuestNativeAsset.h"
void CompileQuestAdapter(RC::Unreal::UObject* controller,RC::Unreal::UObject* quest) {
    DragonWilds::QuestNative::Adapter adapter(controller,quest);
    adapter.ValidateAll();
    adapter.IsInitialized();
    adapter.StateName();
    adapter.SetGiven(false);
    adapter.SetObjective(RC::Unreal::FName(TEXT("test"),RC::Unreal::FNAME_Add));
    adapter.SetComplete(false);
}
uint16_t CompileQuestRegistry(RC::Unreal::UObject* subsystem,RC::Unreal::UObject* instance,RC::Unreal::UObject* quest) {
    return DragonWilds::QuestRegistry::NativeRegistry::Register(subsystem,instance,quest);
}
uint16_t CompileJournalRegistry(RC::Unreal::UObject* subsystem,RC::Unreal::UObject* instance,RC::Unreal::UObject* entry) {
    return DragonWilds::QuestRegistry::NativeRegistry::RegisterJournal(subsystem,instance,entry);
}
void CompileQuestAsset(const DragonWilds::Quests::Definition& definition) {
    DragonWilds::Quests::NativeAsset asset(definition);
    asset.Get();
}
