#include "Generator/SaveViewer.h"
#include "Generator/SaveReport.h"
#include "Generator/NpcSaveExport.h"
#include "Generator/ToolRequest.h"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/UEnum.hpp"
#include "Core/SaveCleanup.h"
#include "Runtime/HostServices.h"
#include <imgui.h>
#include <set>
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/FText.hpp"
#include "Helpers/String.hpp"
#include <Windows.h>
#include <filesystem>
#include <cstring>
#include <format>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <vector>

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;
using nlohmann::json;
namespace fs=std::filesystem;
namespace PS::SaveViewer {
namespace {
    fs::path ImportFolder() {
        const auto folder=HostServices::JobsDirectory()/"imports/saves";
        fs::create_directories(folder);
        return fs::weakly_canonical(folder);
    }
    std::vector<fs::path> ImportedSaves() {
        const auto folder=ImportFolder();std::vector<fs::path> files;size_t entries=0;
        for(const auto& entry:fs::directory_iterator(folder)) {
            if(++entries>256)throw std::runtime_error("Save import folder exceeds 256 entries");
            if(!entry.is_regular_file() || entry.path().extension()!=L".json")continue;
            const auto path=fs::weakly_canonical(entry.path());
            if(path.parent_path()!=folder)throw std::runtime_error("Save import path escaped its folder");
            files.push_back(path);
        }
        std::ranges::sort(files,[](const auto& left,const auto& right){return left.filename()<right.filename();});
        return files;
    }
    bool ImportSelector(const char* label,char* destination,size_t capacity) {
        const auto folder=ImportFolder();bool changed=false;
        ImGui::TextWrapped("Shared save input: %s",folder.string().c_str());
        auto current=destination[0]?fs::path(std::u8string(destination,destination+std::strlen(destination))).filename().string():std::string("Select imported save");
        if(ImGui::BeginCombo(label,current.c_str())) {
            for(const auto& file:ImportedSaves()) {
                const auto utf8=file.u8string();const std::string value(utf8.begin(),utf8.end());
                if(ImGui::Selectable(file.filename().string().c_str(),value==destination)) {
                    if(value.size()>=capacity)throw std::runtime_error("Imported save path exceeds UI capacity");
                    std::snprintf(destination,capacity,"%s",value.c_str());changed=true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }
    json Read(const fs::path& path) {
        const auto size=fs::file_size(path);
        if(size>8*1024*1024)throw std::runtime_error("Save-viewer file limit: 8 MiB.");
        const auto modified=fs::last_write_time(path);
        std::ifstream stream(path,std::ios::binary);
        if(!stream)throw std::runtime_error("Cannot open save for reading.");
        std::string text(static_cast<size_t>(size),'\0');
        stream.read(text.data(),static_cast<std::streamsize>(size));
        if(!stream || stream.peek()!=std::char_traits<char>::eof() || fs::last_write_time(path)!=modified)
            throw std::runtime_error("Save changed during read; retry after saving finishes.");
        if(text.size()>=2 && static_cast<unsigned char>(text[0])==0xff && static_cast<unsigned char>(text[1])==0xfe) {
            if((text.size()-2)%2)throw std::runtime_error("Truncated UTF-16 save.");
            std::wstring wide((text.size()-2)/2,L'\0');std::memcpy(wide.data(),text.data()+2,text.size()-2);
            const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),static_cast<int>(wide.size()),nullptr,0,nullptr,nullptr);
            if(count<=0)throw std::runtime_error("Invalid UTF-16 save.");
            text.assign(count,'\0');
            if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),static_cast<int>(wide.size()),text.data(),count,nullptr,nullptr))
                throw std::runtime_error("Save text conversion failed.");
        }
        size_t nodes=0;
        return json::parse(text,[&](int depth,json::parse_event_t,json&){
            if(depth>64 || ++nodes>262144)throw std::runtime_error("Save structure exceeds viewer limits.");return true;
        });
    }
    FProperty* TextField(UObject* object,const TCHAR* name) {
        if(!object || !object->GetClassPrivate())return nullptr;
        auto* field=PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name);
        const auto size=object->GetClassPrivate()->GetPropertiesSize();
        if(!field)return nullptr;
        const auto offset=field->GetOffset_Internal(),width=field->GetElementSize();
        if(field->GetArrayDim()!=1 || offset<0 || width<=0 || offset>size || width>size-offset)
            throw std::runtime_error("Unsupported save-viewer text layout.");
        return field;
    }
    std::string BoundedString(const FString& value) {
        const auto& chars=value.GetCharArray();
        if(chars.Num()<0 || chars.Num()>4097 || (chars.Num() && !chars.GetData()))
            throw std::runtime_error("Save-viewer string exceeds limits.");
        if(!chars.Num())return {};
        if(chars.GetData()[chars.Num()-1]!=0)throw std::runtime_error("Unterminated save-viewer string.");
        return to_string(StringType(chars.GetData(),chars.Num()-1));
    }
    std::string StringField(UObject* object,const TCHAR* name) {
        auto* field=CastField<FStrProperty>(TextField(object,name));
        if(!field)return {};
        if(field->GetElementSize()!=sizeof(FString))throw std::runtime_error("Unsupported save-viewer string layout.");
        return BoundedString(*field->ContainerPtrToValuePtr<FString>(object));
    }
    std::string DisplayName(UObject* object) {
        for(const auto* name:{TEXT("Name"),TEXT("DisplayName")}) {
            auto* field=CastField<FTextProperty>(TextField(object,name));
            if(!field)continue;
            if(field->GetElementSize()!=sizeof(FText))throw std::runtime_error("Unsupported save-viewer name layout.");
            const auto& text=*field->ContainerPtrToValuePtr<FText>(object);
            const auto display=text.ToString();
            if(display.size()>4096)throw std::runtime_error("Save-viewer name exceeds limits.");
            const auto value=to_string(display);
            if(!value.empty())return value;
        }
        return {};
    }
    void Resolve(json& rows,const TCHAR* classPath) {
        auto* cls=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,classPath);
        if(!cls)throw std::runtime_error("Item/building class unavailable; enter a world first.");
        std::unordered_map<std::string,std::vector<size_t>> wanted;
        for(size_t i=0;i<rows.size();++i)if(rows[i].contains("PersistenceID") && rows[i]["PersistenceID"].is_string())wanted[rows[i]["PersistenceID"].get<std::string>()].push_back(i);
        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(cls,objects,true);
        if(objects.Num()>32768)throw std::runtime_error("Loaded asset inventory exceeds viewer limit.");
        std::unordered_map<std::string,bool> seen;
        for(auto* object:objects) {
            if(!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization)))continue;
            const auto id=StringField(object,TEXT("PersistenceID"));
            const auto found=wanted.find(id);if(found==wanted.end())continue;
            const auto path=to_string(object->GetPathName());
            for(auto index:found->second) {
                auto& row=rows[index];
                if(seen.contains(id)) {
                    row["DisplayName"]=nullptr;row["InternalName"]=nullptr;row["AssetPath"]=nullptr;row["Origin"]="Ambiguous PersistenceID";
                }else {
                    row["DisplayName"]=DisplayName(object);row["InternalName"]=StringField(object,TEXT("InternalName"));row["AssetPath"]=path;
                    row["Origin"]=SaveReport::Origin(path);
                }
            }
            seen[id]=true;
        }
    }
    json Character(UObject* controller) {
        ActorHelper::FunctionCall call(controller,TEXT("/Script/Dominion.DominionPlayerControllerBase:GetCharacterGuid"));
        call.Invoke();uint32 lanes[4]{};call.MoveResult(lanes,sizeof(lanes));
        const auto guid=SaveReport::Guid(std::format("{:08X}{:08X}{:08X}{:08X}",lanes[0],lanes[1],lanes[2],lanes[3]));
        if(guid.empty())throw std::runtime_error("Active character identity unavailable.");
        wchar_t local[MAX_PATH]{};
        const auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);
        if(!length || length>=MAX_PATH)throw std::runtime_error("Character-save directory unavailable.");
        const auto folder=fs::path(local)/"RSDragonwilds/Saved/SaveCharacters";
        json result={{"Kind","RuneSchemaSavedItems1"},{"Rows",json::array()},{"Warnings",json::array()},
            {"Coverage","Last saved Inventory and PersonalInventory for the active character; not live inventory or world containers. Origins inferred from asset namespaces, not verified pak ownership."}};
        bool matched=false;size_t files=0,bytes=0,entries=0;
        for(const auto& entry:fs::directory_iterator(folder)) {
            if(++entries>512)throw std::runtime_error("Save directory exceeds 512 entries.");
            if(!entry.is_regular_file() || entry.path().extension()!=".json")continue;
            if(++files>64 || (bytes+=entry.file_size())>64*1024*1024)throw std::runtime_error("Character-save search limit exceeded.");
            json save;
            try {save=Read(entry.path());}
            catch(const std::exception& error){result["Warnings"].push_back(entry.path().filename().string()+": "+error.what());continue;}
            if(!save.contains("meta_data") || !save["meta_data"].is_object() || !save["meta_data"].contains("char_guid") || !save["meta_data"]["char_guid"].is_string())continue;
            if(SaveReport::Guid(save["meta_data"]["char_guid"].get<std::string>())!=guid)continue;
            if(matched)throw std::runtime_error("Multiple saves match the active character; refusing to guess.");
            matched=true;result["SaveFile"]=entry.path().filename().string();result["Rows"]=SaveReport::Inventory(save);
        }
        if(!matched)throw std::runtime_error("No readable local save matches the active character. Save in-game, then retry.");
        Resolve(result["Rows"],TEXT("/Script/Dominion.ItemData"));
        return result;
    }
    json World(UObject* controller) {
        auto* cls=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.PersistenceSubsystem"));
        if(!cls)throw std::runtime_error("World persistence subsystem unavailable.");
        TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(cls,objects,true);
        UObject* persistence=nullptr;
        if(objects.Num()>64)throw std::runtime_error("World subsystem inventory exceeds viewer limit.");
        for(auto* object:objects)if(object && object->GetWorld()==controller->GetWorld() && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject))) {
            if(persistence)throw std::runtime_error("Ambiguous world persistence subsystem.");persistence=object;
        }
        if(!persistence)throw std::runtime_error("Active world save unavailable on this client.");
        auto* settings=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(persistence->GetClassPrivate(),TEXT("WorldSaveSettings")));
        auto* guid=settings?CastField<FStructProperty>(PropertyHelper::GetPropertyByName(settings->GetStruct().Get(),TEXT("WorldSaveGuid"))):nullptr;
        if(!guid || guid->GetElementSize()!=16)throw std::runtime_error("Unsupported world save identity layout.");
        uint32 lanes[4]{};std::memcpy(lanes,guid->ContainerPtrToValuePtr<void>(settings->ContainerPtrToValuePtr<void>(persistence)),sizeof(lanes));
        const auto id=std::format("{:08X}-{:08X}-{:08X}-{:08X}",lanes[0],lanes[1],lanes[2],lanes[3]);
        if(SaveReport::Guid(id).empty())throw std::runtime_error("Active world GUID unavailable.");
        auto* library=ActorHelper::ResolveObject(TEXT("/Script/Engine.Default__KismetSystemLibrary"));
        ActorHelper::FunctionCall saved(library,TEXT("/Script/Engine.KismetSystemLibrary:GetProjectSavedDirectory"));
        saved.Invoke();const auto directory=saved.Result<FString>();
        if(directory.GetCharArray().Num()<=1)throw std::runtime_error("Project save directory unavailable.");
        const auto path=fs::path(*directory)/"RuneSchema"/id/"CustomBuildingData.json";
        if(!fs::exists(path))throw std::runtime_error("No RuneSchema building registry exists for this world.");
        const auto data=Read(path);
        if(!data.contains("Records") || !data["Records"].is_array() || data["Records"].size()>4096)throw std::runtime_error("Unsupported building registry layout or size.");
        json rows=json::array();
        for(const auto& record:data["Records"]) {
            if(!record.is_object() || !record.contains("PersistenceID") || !record["PersistenceID"].is_string())
                throw std::runtime_error("Invalid building registry record.");
            json row={{"Origin","RuneSchema world registry"},{"DisplayName",nullptr}};
            for(const auto* key:{"Owner","Key","PersistenceID","InternalName","AssetPath","State","HistoricalIndex"})
                if(record.contains(key))row[key]=record[key];
            rows.push_back(std::move(row));
        }
        const auto historical=rows;
        Resolve(rows,TEXT("/Script/Dominion.BuildingPieceData"));
        for(size_t i=0;i<rows.size();++i) {
            if(historical[i].contains("AssetPath"))rows[i]["RegisteredAssetPath"]=historical[i]["AssetPath"];
            if(historical[i].contains("InternalName"))rows[i]["RegisteredInternalName"]=historical[i]["InternalName"];
            rows[i]["Resolution"]=rows[i].value("Origin",std::string("Unresolved"));
            rows[i]["Origin"]="RuneSchema world registry";
            if(!rows[i].contains("InternalName") || rows[i]["InternalName"].is_null())if(historical[i].contains("InternalName"))rows[i]["InternalName"]=historical[i]["InternalName"];
        }
        return {{"Kind","RuneSchemaWorldRegistry1"},{"Rows",rows},{"Warnings",json::array()},
            {"Coverage","Active-world RuneSchema building registration history only. Not proof of placed instances; no containers, terrain or full world save are scanned."}};
    }
}
#include "SaveCleanupPanel.inl"
#include "NpcExportPanel.inl"
json Capture(UObject* controller,bool world) {
    if(!controller || !controller->GetWorld())throw std::runtime_error("Enter a world first.");
    auto report=world?World(controller):Character(controller);
    report["CapturedAtUnixMs"]=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return report;
}
}
