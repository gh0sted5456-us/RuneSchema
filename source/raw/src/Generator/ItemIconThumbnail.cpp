#include "Generator/ItemIconThumbnail.h"
#include "Runtime/HostServices.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/World.hpp"
#include "Helpers/Casting.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdexcept>
#include <vector>

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;

namespace PS::ItemIconThumbnail {
namespace {
    constexpr int ThumbnailSize=64;

    class ReflectedCall {
    public:
        ReflectedCall(UObject* self,const TCHAR* path):m_self(self) {
            if(!m_self)throw std::runtime_error("Item icon renderer has no function owner");
            m_function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
            if(!m_function)throw std::runtime_error(std::format("Item icon renderer function '{}' is unavailable",RC::to_string(StringType(path))));
            if(m_function->GetParmsSize()<0 || m_function->GetParmsSize()>65535)throw std::runtime_error("Item icon renderer function parameter layout is invalid");
            m_params.resize(static_cast<size_t>(m_function->GetParmsSize()));
            for(auto* property:TFieldRange<FProperty>(m_function,EFieldIterationFlags::Default)) {
                if(!property->HasAnyPropertyFlags(CPF_Parm) || property->GetOffset_Internal()<0
                    || static_cast<size_t>(property->GetOffset_Internal())+static_cast<size_t>(property->GetElementSize())>m_params.size())continue;
                property->InitializeValue_InContainer(m_params.data());m_initialized.push_back(property);
            }
        }
        ~ReflectedCall(){for(auto it=m_initialized.rbegin();it!=m_initialized.rend();++it)try{(*it)->DestroyValue_InContainer(m_params.data());}catch(...) {}}
        ReflectedCall(const ReflectedCall&)=delete;ReflectedCall& operator=(const ReflectedCall&)=delete;

        FProperty* Property(const TCHAR* name) const {
            auto* property=m_function->FindProperty(FName(name,FNAME_Find));
            if(!property || !property->HasAnyPropertyFlags(CPF_Parm) || property->GetOffset_Internal()<0)
                throw std::runtime_error(std::format("Item icon renderer parameter '{}' changed",RC::to_string(StringType(name))));
            return property;
        }
        void* Value(const TCHAR* name){auto* property=Property(name);return property->ContainerPtrToValuePtr<void>(m_params.data());}
        const void* Value(const TCHAR* name) const {auto* property=Property(name);return property->ContainerPtrToValuePtr<void>(const_cast<uint8_t*>(m_params.data()));}
        bool Has(const TCHAR* name) const {auto* property=m_function->FindProperty(FName(name,FNAME_Find));return property && property->HasAnyPropertyFlags(CPF_Parm);}
        ReflectedCall& ObjectArg(const TCHAR* name,UObject* value) {
            auto* property=CastField<FObjectProperty>(Property(name));
            if(!property || property->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Item icon object parameter layout changed");
            auto* address=property->ContainerPtrToValuePtr<void>(m_params.data());
            if(!address)throw std::runtime_error("Item icon object parameter address is unavailable");
            std::memcpy(address,&value,sizeof(value));
            return *this;
        }
        ReflectedCall& JsonArg(const TCHAR* name,const nlohmann::json& value) {
            auto* property=Property(name);PropertyHelper::CopyJsonValueToContainer(m_params.data(),property,value);return *this;
        }
        ReflectedCall& JsonArgIfPresent(const TCHAR* name,const nlohmann::json& value) {
            if(Has(name))JsonArg(name,value);return *this;
        }
        ReflectedCall& CopyArg(const TCHAR* name,const ReflectedCall& source,const TCHAR* sourceName) {
            auto* destination=Property(name);auto* origin=source.Property(sourceName);
            if(destination->GetClass()!=origin->GetClass() || destination->GetElementSize()!=origin->GetElementSize())
                throw std::runtime_error("Item icon render context layout changed");
            if(auto* ds=CastField<FStructProperty>(destination)) {
                auto* os=CastField<FStructProperty>(origin);if(!os || ds->GetStruct()!=os->GetStruct())throw std::runtime_error("Item icon render context struct changed");
            }
            destination->CopyCompleteValue(destination->ContainerPtrToValuePtr<void>(m_params.data()),source.Value(sourceName));return *this;
        }
        void Invoke(){m_self->ProcessEvent(m_function,m_params.data());}
        UObject* ObjectValue(const TCHAR* name) {
            auto* property=CastField<FObjectProperty>(Property(name));
            if(!property || property->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Item icon output object layout changed");
            UObject* value=nullptr;
            auto* address=property->ContainerPtrToValuePtr<void>(m_params.data());
            if(!address)throw std::runtime_error("Item icon output object address is unavailable");
            std::memcpy(&value,address,sizeof(value));
            return value;
        }
        UObject* ObjectReturn() {
            auto* property=CastField<FObjectProperty>(m_function->GetReturnProperty());
            if(!property || property->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Item icon renderer return object layout changed");
            UObject* value=nullptr;
            auto* address=property->ContainerPtrToValuePtr<void>(m_params.data());
            if(!address)throw std::runtime_error("Item icon return object address is unavailable");
            std::memcpy(&value,address,sizeof(value));
            return value;
        }
        bool BoolReturn() {
            auto* property=CastField<FBoolProperty>(m_function->GetReturnProperty());if(!property)throw std::runtime_error("Item icon renderer bool return layout changed");
            return property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(m_params.data()));
        }
    private:
        UObject* m_self{};UFunction* m_function{};std::vector<uint8_t> m_params;std::vector<FProperty*> m_initialized;
    };

    UObject* DefaultRenderingLibrary() {
        auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetRenderingLibrary"),false);
        if(!object)throw std::runtime_error("KismetRenderingLibrary is unavailable");return object;
    }

    uint64_t HashPath(const std::string& value) {
        uint64_t hash=1469598103934665603ull;for(unsigned char byte:value){hash^=byte;hash*=1099511628211ull;}return hash;
    }

    std::filesystem::path OutputFile(const std::string& iconPath) {
        auto folder=HostServices::CacheDirectory()/"item-icons";std::filesystem::create_directories(folder);
        return folder/(std::format("{:016x}.rsicon",HashPath(iconPath)));
    }

    UObject* CreateThumbnailRenderTarget(UObject* library,UWorld* world) {
        std::string lastError;
        for(const auto* format:{"RTF_RGBA8_SRGB","RTF_RGBA8"}) {
            try {
                ReflectedCall create(library,TEXT("/Script/Engine.KismetRenderingLibrary:CreateRenderTarget2D"));
                create.ObjectArg(TEXT("WorldContextObject"),world).JsonArg(TEXT("Width"),ThumbnailSize).JsonArg(TEXT("Height"),ThumbnailSize)
                    .JsonArg(TEXT("Format"),format).JsonArg(TEXT("ClearColor"),{{"R",0.0},{"G",0.0},{"B",0.0},{"A",0.0}})
                    .JsonArgIfPresent(TEXT("bAutoGenerateMipMaps"),false).JsonArgIfPresent(TEXT("bSupportUAVs"),false).Invoke();
                if(auto* target=create.ObjectReturn())return target;
                lastError="CreateRenderTarget2D returned null";
            } catch(const std::exception& error) {lastError=error.what();}
        }
        throw std::runtime_error("Could not create RGBA8 item icon render target: "+lastError);
    }

    void ReleaseRenderTarget(UObject* library,UObject* target) {
        if(!library || !target)return;
        try {ReflectedCall release(library,TEXT("/Script/Engine.KismetRenderingLibrary:ReleaseRenderTarget2D"));release.ObjectArg(TEXT("TextureRenderTarget"),target).Invoke();}catch(...){}
    }

    std::vector<unsigned char> ReadColors(ReflectedCall& read) {
        auto* arrayProperty=CastField<FArrayProperty>(read.Property(TEXT("OutSamples")));
        auto* inner=arrayProperty?CastField<FStructProperty>(arrayProperty->GetInner()):nullptr;
        UScriptStruct* colorStruct=inner ? inner->GetStruct().Get() : nullptr;
        if(!arrayProperty || !inner || !colorStruct)throw std::runtime_error("ReadRenderTarget FColor array contract changed");
        auto* array=static_cast<FScriptArray*>(read.Value(TEXT("OutSamples")));
        if(!array || array->Num()!=ThumbnailSize*ThumbnailSize || !array->GetData())throw std::runtime_error("ReadRenderTarget returned an unexpected thumbnail size");
        auto channel=[&](const TCHAR* name)->FNumericProperty*{
            FProperty* rawChannelProperty=PropertyHelper::GetPropertyByName(colorStruct,name);
            FNumericProperty* numericChannelProperty=CastField<FNumericProperty>(rawChannelProperty);
            if(!numericChannelProperty || !numericChannelProperty->IsInteger())
                throw std::runtime_error("FColor channel layout changed");
            return numericChannelProperty;
        };
        auto* red=channel(TEXT("R"));auto* green=channel(TEXT("G"));auto* blue=channel(TEXT("B"));auto* alpha=channel(TEXT("A"));
        const auto stride=inner->GetElementSize();if(stride<=0 || stride>64)throw std::runtime_error("FColor stride is invalid");
        std::vector<unsigned char> rgba(static_cast<size_t>(array->Num())*4);
        for(int32_t index=0;index<array->Num();++index) {
            auto* sample=static_cast<uint8_t*>(array->GetData())+static_cast<size_t>(index)*static_cast<size_t>(stride);
            const auto readChannel=[&](FNumericProperty* property){return static_cast<unsigned char>(std::clamp<int64_t>(property->GetUnsignedIntPropertyValue(property->ContainerPtrToValuePtr<void>(sample)),0,255));};
            rgba[static_cast<size_t>(index)*4+0]=readChannel(red);
            rgba[static_cast<size_t>(index)*4+1]=readChannel(green);
            rgba[static_cast<size_t>(index)*4+2]=readChannel(blue);
            rgba[static_cast<size_t>(index)*4+3]=readChannel(alpha);
        }
        return rgba;
    }

    void WriteThumbnail(const std::filesystem::path& file,const std::vector<unsigned char>& rgba) {
        if(rgba.size()!=ThumbnailSize*ThumbnailSize*4)throw std::runtime_error("Item icon thumbnail byte count is invalid");
        const auto temporary=file.wstring()+L".tmp";
        std::ofstream output(std::filesystem::path(temporary),std::ios::binary|std::ios::trunc);
        if(!output)throw std::runtime_error("Cannot create item icon thumbnail cache file");
        const uint32_t width=ThumbnailSize,height=ThumbnailSize,bytes=static_cast<uint32_t>(rgba.size());
        output.write("RSI1",4);output.write(reinterpret_cast<const char*>(&width),sizeof(width));
        output.write(reinterpret_cast<const char*>(&height),sizeof(height));output.write(reinterpret_cast<const char*>(&bytes),sizeof(bytes));
        output.write(reinterpret_cast<const char*>(rgba.data()),static_cast<std::streamsize>(rgba.size()));output.close();
        if(!output)throw std::runtime_error("Item icon thumbnail write failed");
        std::error_code ec;std::filesystem::remove(file,ec);ec.clear();std::filesystem::rename(temporary,file,ec);
        if(ec)throw std::runtime_error("Item icon thumbnail could not be committed");
    }
}

nlohmann::json Render(UWorld* world,const std::string& iconPath) {
    if(!world || iconPath.empty() || iconPath.size()>1024 || iconPath.front()!='/')throw std::runtime_error("Item icon request requires a loaded world and canonical texture path");
    auto* texture=ActorHelper::ResolveObject(RC::to_generic_string(iconPath));
    auto* textureType=ActorHelper::ResolveClass(TEXT("/Script/Engine.Texture2D"));
    if(!texture || !textureType || !texture->IsA(textureType)
        || texture->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))
        throw std::runtime_error("Item icon texture is unavailable or is not Texture2D");
    auto* library=DefaultRenderingLibrary();UObject* renderTarget=nullptr;
    try {
        renderTarget=CreateThumbnailRenderTarget(library,world);

        ReflectedCall clear(library,TEXT("/Script/Engine.KismetRenderingLibrary:ClearRenderTarget2D"));
        clear.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),renderTarget)
            .JsonArg(TEXT("ClearColor"),{{"R",0.0},{"G",0.0},{"B",0.0},{"A",0.0}}).Invoke();

        ReflectedCall begin(library,TEXT("/Script/Engine.KismetRenderingLibrary:BeginDrawCanvasToRenderTarget"));
        begin.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),renderTarget).Invoke();
        auto* canvas=begin.ObjectValue(TEXT("Canvas"));if(!canvas)throw std::runtime_error("BeginDrawCanvasToRenderTarget returned no Canvas");
        bool ended=false;
        try {
            ReflectedCall draw(canvas,TEXT("/Script/Engine.Canvas:K2_DrawTexture"));
            draw.ObjectArg(TEXT("RenderTexture"),texture)
                .JsonArg(TEXT("ScreenPosition"),{{"X",0.0},{"Y",0.0}})
                .JsonArg(TEXT("ScreenSize"),{{"X",ThumbnailSize},{"Y",ThumbnailSize}})
                .JsonArg(TEXT("CoordinatePosition"),{{"X",0.0},{"Y",0.0}})
                .JsonArg(TEXT("CoordinateSize"),{{"X",1.0},{"Y",1.0}})
                .JsonArg(TEXT("RenderColor"),{{"R",1.0},{"G",1.0},{"B",1.0},{"A",1.0}})
                .JsonArg(TEXT("BlendMode"),"BLEND_Translucent").JsonArg(TEXT("Rotation"),0.0)
                .JsonArg(TEXT("PivotPoint"),{{"X",0.5},{"Y",0.5}}).Invoke();
            ReflectedCall end(library,TEXT("/Script/Engine.KismetRenderingLibrary:EndDrawCanvasToRenderTarget"));
            end.ObjectArg(TEXT("WorldContextObject"),world).CopyArg(TEXT("Context"),begin,TEXT("Context")).Invoke();ended=true;
        } catch(...) {
            if(!ended)try {ReflectedCall end(library,TEXT("/Script/Engine.KismetRenderingLibrary:EndDrawCanvasToRenderTarget"));end.ObjectArg(TEXT("WorldContextObject"),world).CopyArg(TEXT("Context"),begin,TEXT("Context")).Invoke();}catch(...){}
            throw;
        }

        ReflectedCall read(library,TEXT("/Script/Engine.KismetRenderingLibrary:ReadRenderTarget"));
        read.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),renderTarget).JsonArg(TEXT("bNormalize"),false).Invoke();
        if(!read.BoolReturn())throw std::runtime_error("ReadRenderTarget failed for item icon");
        auto rgba=ReadColors(read);const auto file=OutputFile(iconPath);WriteThumbnail(file,rgba);ReleaseRenderTarget(library,renderTarget);renderTarget=nullptr;
        return {{"Icon",iconPath},{"File",file.string()},{"Width",ThumbnailSize},{"Height",ThumbnailSize},{"Status","Ready"}};
    } catch(...) {ReleaseRenderTarget(library,renderTarget);throw;}
}
}
