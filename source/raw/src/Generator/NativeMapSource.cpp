#include "Generator/NativeMapSource.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/World.hpp"
#include "Helpers/Casting.hpp"
namespace PS::NativeMapSource {
namespace {
using namespace RC;using namespace RC::Unreal;namespace AH=DragonWilds::ActorHelper;namespace PH=DragonWilds::PropertyHelper;
constexpr const char* WorldTexture="/Game/Art/UI/Map/T_WorldMap_Landmass.T_WorldMap_Landmass";
bool InWorld(UObject* object,UWorld* world){for(int i=0;object&&i<32;++i,object=object->GetOuterPrivate())if(object==world)return true;return false;}
UObject* Background(UWorld* world){
    auto* type=AH::ResolveClass(TEXT("/Script/MinimapPlugin.MapBackground"));TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(type,objects,true);
    if(objects.Num()<0||objects.Num()>128)throw std::runtime_error("Native map background roster exceeds its bound");
    for(auto* object:objects){if(!object||!InWorld(object,world)||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        auto* array=CastField<FArrayProperty>(PH::GetPropertyByName(object->GetClassPrivate(),TEXT("BackgroundLevels")));if(!array)continue;
        auto* inner=CastField<FStructProperty>(array->GetInner());if(!inner||!inner->GetStruct())continue;
        auto* texture=CastField<FObjectProperty>(PH::GetPropertyByName(inner->GetStruct().Get(),TEXT("BackgroundTexture")));if(!texture)continue;
        auto* data=array->ContainerPtrToValuePtr<FScriptArray>(object);if(!data||data->Num()<1||data->Num()>16||!data->GetData())continue;
        for(int i=0;i<data->Num();++i){auto* row=static_cast<uint8*>(data->GetData())+i*inner->GetElementSize();auto* image=texture->GetObjectPropertyValue(texture->ContainerPtrToValuePtr<void>(row));
            if(image&&RC::to_string(image->GetPathName())==WorldTexture)return object;}
    }throw std::runtime_error("The native Ashenfall map is not loaded in this world");
}
FVector Vector(UObject* object,const TCHAR* function){AH::FunctionCall call(object,function);call.Invoke();return call.Result<FVector>();}

}
Source Read(UWorld* world){
    if(!world)throw std::runtime_error("Load a world to read the map");auto* background=Background(world);
    auto* area=AH::GetObjectRef(background,TEXT("AreaBounds"));auto* capture=AH::GetObjectRef(background,TEXT("CaptureComponent2D"));
    const auto center=Vector(area,TEXT("/Script/Engine.SceneComponent:K2_GetComponentLocation"));
    const auto extent=Vector(area,TEXT("/Script/Engine.BoxComponent:GetScaledBoxExtent"));
    const auto right=Vector(capture,TEXT("/Script/Engine.SceneComponent:GetRightVector")),up=Vector(capture,TEXT("/Script/Engine.SceneComponent:GetUpVector"));
    const auto areaX=Vector(area,TEXT("/Script/Engine.SceneComponent:GetForwardVector")),areaY=Vector(area,TEXT("/Script/Engine.SceneComponent:GetRightVector"));
    if(std::abs(right.Z())>0.01||std::abs(up.Z())>0.01)throw std::runtime_error("Native map capture is not aligned to the world plane");
    MapProjection::Projection result;
    result.Center={center.X(),center.Y()};result.Right={right.X(),right.Y()};result.Up={up.X(),up.Y()};
    result.HalfWidth=std::abs(right.X()*areaX.X()+right.Y()*areaX.Y())*extent.X()+std::abs(right.X()*areaY.X()+right.Y()*areaY.Y())*extent.Y();
    result.HalfHeight=std::abs(up.X()*areaX.X()+up.Y()*areaX.Y())*extent.X()+std::abs(up.X()*areaY.X()+up.Y()*areaY.Y())*extent.Y();result.MinZ=center.Z()-extent.Z();result.MaxZ=center.Z()+extent.Z();
    if(!result.Valid())throw std::runtime_error("Native map bounds or camera axes are invalid");return {result,AH::ResolveObject(RC::to_generic_string(WorldTexture))};
}

}