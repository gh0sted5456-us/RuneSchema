#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JobBoardScrollAuthoring.generated.h"

UCLASS()
class RUNESCHEMABRIDGEAUTHORING_API UJobBoardScrollAuthoring : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="JobBoard|Authoring")
    static bool AuthorScrollBlueprint();
};
