#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RuneSchemaBridgeAuthoringLibrary.generated.h"

// Explicit editor/commandlet entry point. Cooking must never rely on a delayed
// editor ticker to repair and save a transport contract before serialization.
UCLASS()
class URuneSchemaBridgeAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="RuneSchema|Authoring")
    static bool AuthorNetworkBridges();
    UFUNCTION(BlueprintCallable, Category="RuneSchema|Authoring")
    static FString InspectIdentityContract();
};
