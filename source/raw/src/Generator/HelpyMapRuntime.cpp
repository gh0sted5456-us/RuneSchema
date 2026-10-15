#include "Generator/HelpyMapRuntime.h"
#include "Generator/HelpyMapTexture.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include <Unreal/Property/FSoftObjectProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <set>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/UnrealCoreStructs.hpp>
#include <Unreal/World.hpp>
#include <algorithm>
#include <filesystem>
namespace PS::HelpyMapRuntime {
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
nlohmann::json PlayerLocations(UWorld* world){
    auto rows=nlohmann::json::array();auto* type=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));if(!type)return rows;
    TArray<UObject*> players;UECustom::UObjectGlobals::GetObjectsOfClass(type,players,true);
    if(players.Num()<0||players.Num()>256)throw std::runtime_error("Native map player roster exceeds its bound");
    // Replicated pawns also exist on clients, where remote controllers do not.
    for(auto* player:players){
        if(!player||!player->IsA<AActor>()||!InWorld(player,world)||player->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        try{
            auto* state=AH::GetObjectRef(player,TEXT("PlayerState"));if(!state)continue;
            AH::FunctionCall getName(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));getName.Invoke();
            const auto value=getName.Result<FString>();const auto& chars=value.GetCharArray();std::string name="Player";
            if(chars.Num()>1&&chars.Num()<256&&chars.GetData()&&chars.GetData()[chars.Num()-1]==0)name=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
            const auto point=AH::GetActorLocation(static_cast<AActor*>(player));if(!std::isfinite(point.X())||!std::isfinite(point.Y())||!std::isfinite(point.Z()))continue;
            rows.push_back({{"Path",RC::to_string(player->GetPathName())},{"Name",std::move(name)},{"Location",{point.X(),point.Y(),point.Z()}}});
        }catch(const std::exception&){} // One despawning pawn must not hide the map.
    }return rows;
}
}
MapProjection::Projection Projection(UWorld* world){
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
    if(!result.Valid())throw std::runtime_error("Native map bounds or camera axes are invalid");return result;
}
nlohmann::json Snapshot(UWorld* world,const nlohmann::json& players){
    const auto alignment=Projection(world);static nlohmann::json readyImage;
    if(!readyImage.is_object()||readyImage.value("Status",std::string{})!="Ready"||!std::filesystem::is_regular_file(readyImage.value("File",std::string{})))readyImage=HelpyMapTexture::Render(world,WorldTexture);
    auto image=readyImage;
    auto locations=PlayerLocations(world);if(locations.empty())locations=players;
    return {{"BridgeVersion",1},{"World",RC::to_string(world->GetPathName())},{"Projection",alignment.ToJson()},{"Players",std::move(locations)},{"Image",std::move(image)}};
}
nlohmann::json Buildings(const std::string& query,std::size_t offset){
    if(query.size()>256||offset>4096)throw std::runtime_error("Building search exceeds its bounds");
    auto* catalogue=AH::ResolveObject(TEXT("/Game/Gameplay/BaseBuilding_New/BuildingPieces/DA_BuildPieceCatalogue_Default.DA_BuildPieceCatalogue_Default"));
    if(!catalogue)throw std::runtime_error("The game's building menu is not loaded");
    auto* pagesProperty=CastField<FArrayProperty>(PH::GetPropertyByName(catalogue->GetClassPrivate(),TEXT("Pages")));
    auto* pageProperty=pagesProperty?CastField<FStructProperty>(pagesProperty->GetInner()):nullptr;
    auto* pageType=pageProperty?pageProperty->GetStruct().Get():nullptr;
    auto* groupsProperty=pageType?CastField<FArrayProperty>(PH::GetPropertyByName(pageType,TEXT("Collection"))):nullptr;
    auto* groupProperty=groupsProperty?CastField<FStructProperty>(groupsProperty->GetInner()):nullptr;
    auto* groupType=groupProperty?groupProperty->GetStruct().Get():nullptr;
    auto* piecesProperty=groupType?CastField<FArrayProperty>(PH::GetPropertyByName(groupType,TEXT("Collection"))):nullptr;
    if(!pageType||!groupType||!piecesProperty||!CastField<FSoftObjectProperty>(piecesProperty->GetInner())||piecesProperty->GetInner()->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr))throw std::runtime_error("Native building menu layout changed");
    const auto array=[](FArrayProperty* property,void* object,int limit){auto* value=property->ContainerPtrToValuePtr<FScriptArray>(object);if(!value||value->Num()<0||value->Num()>limit||(value->Num()&&!value->GetData()))throw std::runtime_error("Native building menu array exceeds its bounds");return value;};
    const auto fold=[](std::string text){for(auto& ch:text)ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));return text;};
    const auto filter=fold(query);auto rows=nlohmann::json::array();std::set<std::string> seen;size_t matched=0;auto* pages=array(pagesProperty,catalogue,64);
    for(int p=0;p<pages->Num();++p){auto* page=static_cast<uint8*>(pages->GetData())+p*pageProperty->GetElementSize();auto* groups=array(groupsProperty,page,256);
        for(int g=0;g<groups->Num();++g){auto* group=static_cast<uint8*>(groups->GetData())+g*groupProperty->GetElementSize();auto* pieces=array(piecesProperty,group,4096);
            for(int i=0;i<pieces->Num();++i){const auto& soft=*reinterpret_cast<UECustom::FSoftObjectPtr*>(static_cast<uint8*>(pieces->GetData())+i*piecesProperty->GetInner()->GetElementSize());
                const auto path=RC::to_string(soft.ObjectID.GetLongPackageFName().ToString()+TEXT(".")+soft.ObjectID.GetAssetFName().ToString());
                if(path.empty()||path.front()!='/'||!seen.insert(path).second)continue;if(seen.size()>4096)throw std::runtime_error("Building menu exceeds 4096 pieces");
                if(!filter.empty()&&fold(path).find(filter)==std::string::npos)continue;if(matched++<offset)continue;
                if(rows.size()>=32)return {{"Rows",rows},{"NextOffset",offset+rows.size()},{"HasMore",true}};
                auto name=path.substr(path.find_last_of('.')+1);auto* piece=AH::ResolveObject(RC::to_generic_string(path));
                if(piece)if(auto* property=CastField<FTextProperty>(PH::GetPropertyByName(piece->GetClassPrivate(),TEXT("DisplayName")))){if(auto* text=property->ContainerPtrToValuePtr<FText>(piece)){const auto native=RC::to_string(PH::GetTextAsString(*text));if(!native.empty())name=native;}}
                rows.push_back({{"Path",path},{"Name",name},{"Type","Building"}});
            }
        }
    }return {{"Rows",rows},{"NextOffset",offset+rows.size()},{"HasMore",false}};
}
}
