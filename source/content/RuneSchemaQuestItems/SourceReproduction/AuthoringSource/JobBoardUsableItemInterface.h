#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DominionPlayerController.h"
#include "JobBoardInventoryComponent.h"
#include "JobBoardUsableItemInterface.generated.h"

// Minimal exact-signature authoring mirror; never shipped as a native DLL.
UINTERFACE(BlueprintType)
class DOMINION_API UUsableItemInterface : public UInterface
{
    GENERATED_BODY()
};

class DOMINION_API IUsableItemInterface
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintPure=false, Category="Item Usage")
    bool UseItem(ADominionPlayerController* PlayerController, UInventoryComponent* Inventory, int32 SlotIndex) const;
};
