#include "Core/HelpySourceCache.h"
#include "Generator/ItemIconThumbnail.h"
#include "Generator/HelpyIconTrace.h"
#include "Generator/HelpyRuntimePreferences.h"
#include "Core/IconCaptureSchedule.h"
#include "Core/IconColorBytes.h"
#include "Core/IconDiskMaintenance.h"
#include "Runtime/SlowWork.h"
#include "Core/IconDrawContext.h"
#include "Utility/EngineCleanupLifetime.h"
#include "SDK/WeakObjectHandle.h"
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"
#include "Utility/Logging.h"
#include "Utility/AssetAliases.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/World.hpp"
#include "Helpers/Casting.hpp"
#include "Helpers/String.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <tuple>
#include <cctype>
#include <chrono>
#include <memory>
#include <cmath>
#include <future>

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;

namespace PS::ItemIconThumbnail {
namespace {
    constexpr int ThumbnailSize=64;
    constexpr std::uintmax_t LiveSessionCacheLimitBytes=32ull*1024ull*1024ull;
    PS::IconDiskMaintenance::Worker DiskMaintenance;
    std::future<void> PendingWrite;
    nlohmann::json PendingWriteReply;
    const auto SessionCacheStarted=std::filesystem::file_time_type::clock::now();


    class ReflectedCall {
    public:
        ReflectedCall(UObject* self,const TCHAR* path):m_self(self) {
            if(!m_self)throw std::runtime_error("Item icon renderer has no function owner");
            m_function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
            if(!m_function)throw std::runtime_error(std::format("Item icon renderer function '{}' is unavailable",RC::to_string(StringType(path))));
            ReflectedContract::Frame(m_function);
            m_params.resize(static_cast<size_t>(m_function->GetParmsSize()));
            for(auto* property:TFieldRange<FProperty>(m_function,EFieldIterationFlags::Default)) {
                if(!property->HasAnyPropertyFlags(CPF_Parm))continue;
                if(!ReflectedContract::Fits(property,static_cast<int>(m_params.size())))throw std::runtime_error("Item icon renderer parameter extent changed");
                property->InitializeValue_InContainer(m_params.data());m_initialized.push_back(property);
            }
        }
        ~ReflectedCall(){for(auto it=m_initialized.rbegin();it!=m_initialized.rend();++it)try{(*it)->DestroyValue_InContainer(m_params.data());}catch(...) {}}
        ReflectedCall(const ReflectedCall&)=delete;ReflectedCall& operator=(const ReflectedCall&)=delete;

        FProperty* Property(const TCHAR* name) const {
            auto* property=m_function->FindProperty(FName(name,FNAME_Find));
            if(!property || !property->HasAnyPropertyFlags(CPF_Parm) || !ReflectedContract::Fits(property,static_cast<int>(m_params.size())))
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
            auto* property=Property(name);WriteJson(m_params.data(),static_cast<int>(m_params.size()),property,value);return *this;
        }
        ReflectedCall& JsonArgIfPresent(const TCHAR* name,const nlohmann::json& value) {
            if(Has(name))JsonArg(name,value);return *this;
        }
        ReflectedCall& TakeDrawContext(ReflectedCall& source) {
            auto* destination=CastField<FStructProperty>(Property(TEXT("Context")));
            auto* origin=CastField<FStructProperty>(source.Property(TEXT("Context")));
            if(!destination||!origin||destination->GetStruct()!=origin->GetStruct()||!destination->GetStruct()
                ||destination->GetStruct()->GetPathName()!=TEXT("/Script/Engine.DrawToRenderTargetContext")
                ||destination->GetElementSize()!=origin->GetElementSize())throw std::runtime_error("Item icon render context struct changed");
            auto* target=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(origin->GetStruct().Get(),TEXT("RenderTarget")));
            const auto size=origin->GetElementSize();
            if(!target||!ReflectedContract::Fits(target,size)||target->GetElementSize()!=sizeof(UObject*)
                ||!target->GetObjectPropertyValue(target->ContainerPtrToValuePtr<void>(source.Value(TEXT("Context")))))
                throw std::runtime_error("Item icon draw context has no render target");
            PS::IconCapture::TransferDrawContext({static_cast<uint8_t*>(Value(TEXT("Context"))),static_cast<size_t>(size)},
                {static_cast<uint8_t*>(source.Value(TEXT("Context"))),static_cast<size_t>(size)},
                destination->HasAnyPropertyFlags(CPF_NoDestructor)&&origin->HasAnyPropertyFlags(CPF_NoDestructor));
            return *this;
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
        static void WriteJson(void* container,int size,FProperty* property,const nlohmann::json& value,int depth=0) {
            if(depth>4||!ReflectedContract::Fits(property,size))throw std::runtime_error("Item icon draw parameter bounds changed");
            if(auto* structure=CastField<FStructProperty>(property);structure&&value.is_object()) {
                auto* type=structure->GetStruct().Get();if(!type)throw std::runtime_error("Item icon draw parameter struct is unavailable");
                auto* address=property->ContainerPtrToValuePtr<void>(container);
                // Resolve each requested key with FName semantics. Enumerating
                // display-case field names and JSON.contains silently skipped
                // native vector/color components when their casing differed.
                for(const auto& [key,field]:value.items()) {
                    auto* child=PropertyHelper::GetPropertyByName(type,RC::to_generic_string(key));
                    if(!child)throw std::runtime_error("Item icon draw field is unavailable: "+key);
                    WriteJson(address,structure->GetElementSize(),child,field,depth+1);
                }
                return;
            }
            PropertyHelper::CopyJsonValueToContainer(container,property,value);
            if(auto* numeric=CastField<FNumericProperty>(property);numeric&&value.is_number()) {
                auto* address=property->ContainerPtrToValuePtr<void>(container);
                const double actual=numeric->IsFloatingPoint()?numeric->GetFloatingPointPropertyValue(address)
                    :static_cast<double>(numeric->GetSignedIntPropertyValue(address));
                const auto expected=value.get<double>();
                if(!std::isfinite(actual)||std::abs(actual-expected)>0.0001)
                    throw std::runtime_error("Item icon draw parameter write was not confirmed");
            }
        }
        UObject* m_self{};UFunction* m_function{};std::vector<uint8_t> m_params;std::vector<FProperty*> m_initialized;
    };

    UObject* DefaultRenderingLibrary() {
        static UObject* cached=nullptr;
        if(cached&&!cached->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return cached;
        cached=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetRenderingLibrary"),false);
        if(!cached)throw std::runtime_error("KismetRenderingLibrary is unavailable");return cached;
    }

    int ConfiguredCacheLimitMB() {
        static int cached=200;static bool haveStamp=false;static std::filesystem::file_time_type stamp{};static std::chrono::steady_clock::time_point next{};
        const auto now=std::chrono::steady_clock::now();if(now<next)return cached;next=now+std::chrono::milliseconds(500);
        const auto file=HelpySourceCache::Directory(false).parent_path().parent_path().parent_path().parent_path()/"ui.jsonc";std::error_code ec;
        const auto currentStamp=std::filesystem::last_write_time(file,ec);if(ec)return cached;if(haveStamp&&currentStamp==stamp)return cached;
        try {const auto doc=nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true);const auto c=doc.value("cache",nlohmann::json::object());cached=std::clamp(c.value("iconCacheMB",200),0,16384);stamp=currentStamp;haveStamp=true;}catch(...){}
        return cached;
    }

    uint64_t HashPath(const std::string& value) {
        uint64_t hash=1469598103934665603ull;for(unsigned char byte:value){hash^=byte;hash*=1099511628211ull;}return hash;
    }

    std::filesystem::path OutputFile(const std::string& iconPath) {
        const bool liveOnly=ConfiguredCacheLimitMB()==0;
        auto folder=HelpySourceCache::IconDirectory(iconPath,liveOnly);
        if(folder.empty())throw std::runtime_error("Icon source is not present in the registered source manifest");
        return folder/(HelpySourceCache::Key(iconPath)+".rsicon");
    }
    bool CachedThumbnail(const std::filesystem::path& file) {
        std::error_code ec;if(!std::filesystem::is_regular_file(file,ec)||ec)return false;
        const auto size=std::filesystem::file_size(file,ec);if(ec||size!=16u+ThumbnailSize*ThumbnailSize*4u)return false;
        std::ifstream input(file,std::ios::binary);if(!input)return false;char magic[4]{};uint32_t w=0,h=0,bytes=0;
        input.read(magic,4);input.read(reinterpret_cast<char*>(&w),sizeof(w));input.read(reinterpret_cast<char*>(&h),sizeof(h));input.read(reinterpret_cast<char*>(&bytes),sizeof(bytes));
        if(!input||std::string(magic,4)!="RSI3"||w!=ThumbnailSize||h!=ThumbnailSize||bytes!=ThumbnailSize*ThumbnailSize*4u)return false;
        std::vector<unsigned char> pixels(bytes);input.read(reinterpret_cast<char*>(pixels.data()),bytes);if(!input)return false;
        std::size_t alphaNonZero=0,rgbNonZero=0;
        for(std::size_t i=0;i+3<pixels.size();i+=4){if(pixels[i+3]!=0)++alphaNonZero;if(pixels[i]!=0||pixels[i+1]!=0||pixels[i+2]!=0)++rgbNonZero;}
        return alphaNonZero>0||rgbNonZero>0;
    }
    void QueueCacheMaintenance(const std::filesystem::path& protectedFile) {
        const int configuredMB=ConfiguredCacheLimitMB();const bool liveOnly=configuredMB==0;
        const auto limit=liveOnly?LiveSessionCacheLimitBytes:static_cast<std::uintmax_t>(configuredMB)*1024ull*1024ull;
        DiskMaintenance.Post({HelpySourceCache::Directory(liveOnly),protectedFile,limit,liveOnly,SessionCacheStarted},HelpyRuntimePreferences::IconCacheEpoch.load());
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

    // Declared before the owners; the barrier below stops Unreal access before
    // their CRT destructors. Weak-object serial checks also require a live
    // FUObjectArray and are not safe after the engine has shut down.
    PS::EngineCleanupLifetime CaptureCleanup;
    struct CaptureLease {
        PS::WeakObjectHandle Handle;bool Owned=false;
        void Hold(UObject* object){
            Handle.Assign(object);
            if(!object||Handle.Get()!=object)throw std::runtime_error("Cannot retain item icon capture object");
            Owned=!object->IsRootSet();if(Owned)object->SetRootSet();
        }
        UObject* Get()const{
            auto* object=Handle.Get();
            return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))?object:nullptr;
        }
        void Release()noexcept{try{CaptureCleanup.Run([&]{if(Owned)if(auto* object=Get();object&&object->IsRootSet())object->ClearRootSet();});}catch(...){}Owned=false;Handle.Reset();}
        ~CaptureLease(){Release();}
    };
    struct Capture {
        std::string Icon,ResolvedClass,ResolvedChain;bool Cold=false;
        PS::WeakObjectHandle World;CaptureLease Texture,Target;
        IconCapture::Schedule Timing;
    };
    std::unique_ptr<Capture> ActiveCapture;
    PS::WeakObjectHandle RendererWorld;
    CaptureLease RendererTarget;
    PS::StopStaticEngineCleanup StopCaptureCleanup{CaptureCleanup};
    UObject* ReusableTarget(UObject* library,UWorld* world) {
        if(RendererWorld.Get()!=world)throw std::runtime_error("Item icon renderer world changed during capture");
        if(auto* target=RendererTarget.Get())return target;
        auto* target=CreateThumbnailRenderTarget(library,world);
        RendererWorld.Assign(world);RendererTarget.Hold(target);
        return target;
    }
    std::uint64_t CaptureNow(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}

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
        const std::array<FNumericProperty*,4> channels{red,green,blue,alpha};
        for(auto* field:channels)if(!ReflectedContract::Fits(field,stride))throw std::runtime_error("FColor channel exceeds its sample stride");
        if(std::all_of(channels.begin(),channels.end(),[](auto* field){return field->GetElementSize()==1&&field->GetArrayDim()==1;})){
            // Validate the reflected byte layout once, not four virtual reads per pixel.
            if(auto bytes=IconCapture::ColorBytes({static_cast<const unsigned char*>(array->GetData()),static_cast<size_t>(array->Num())*stride},
                static_cast<size_t>(array->Num()),static_cast<size_t>(stride),{red->GetOffset_Internal(),green->GetOffset_Internal(),blue->GetOffset_Internal(),alpha->GetOffset_Internal()}))return std::move(*bytes);
        }
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


    UClass* TextureClass() {
        static UClass* cached=nullptr;
        if(cached&&!cached->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return cached;
        cached=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.Texture"),false);return cached;
    }
    bool IsTexture(UObject* object) {auto* textureType=TextureClass();return object&&textureType&&object->IsA(textureType);}
    bool RelevantIconField(std::string name) {
        for(auto& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return name.find("icon")!=std::string::npos||name.find("texture")!=std::string::npos||name.find("image")!=std::string::npos
            ||name.find("brush")!=std::string::npos||name.find("resource")!=std::string::npos||name.find("thumbnail")!=std::string::npos||name.find("asset")!=std::string::npos;
    }
    struct ResolvedTexture {UObject* Texture{};std::string Chain;};
    ResolvedTexture ResolveDrawableObject(UObject* object,int depth,std::unordered_set<UObject*>& visited,const std::string& chain);
    ResolvedTexture ResolveDrawableProperty(FProperty* property,void* container,int containerSize,int depth,std::unordered_set<UObject*>& visited,const std::string& chain) {
        if(!property||!container||depth>5||property->GetArrayDim()!=1)return {};
        const auto offset=property->GetOffset_Internal(),size=property->GetElementSize();
        if(offset<0||size<=0||offset>containerSize||size>containerSize-offset)return {};
        auto* address=property->ContainerPtrToValuePtr<void>(container);const auto propertyName=to_string(property->GetName());const auto next=chain+"."+propertyName;
        if(auto* hard=CastField<FObjectPropertyBase>(property)) {
            if(auto* linked=hard->GetObjectPropertyValue(address))return ResolveDrawableObject(linked,depth+1,visited,next+"->"+to_string(linked->GetPathName()));
        }
        if((CastField<FSoftObjectProperty>(property)||CastField<FSoftClassProperty>(property))&&size==sizeof(UECustom::FSoftObjectPtr)) {
            const auto& soft=*static_cast<UECustom::FSoftObjectPtr*>(address);const auto package=to_string(soft.ObjectID.GetLongPackageFName().ToString()),asset=to_string(soft.ObjectID.GetAssetFName().ToString());
            if(package.starts_with('/')&&!asset.empty()&&asset!="None"&&soft.ObjectID.SubPathString.GetCharArray().Num()<=1) {
                const auto path=package+"."+asset;const auto normalized=PS::AssetAliases::Resolve(ActorHelper::NormalizeObjectPath(RC::to_generic_string(path)));
                auto* linked=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,normalized.c_str(),false);if(!linked)linked=ActorHelper::ResolveObject(normalized);
                if(linked)return ResolveDrawableObject(linked,depth+1,visited,next+"=>"+path);
            }
        }
        if(CastField<FStrProperty>(property)) {
            const auto& chars=static_cast<FString*>(address)->GetCharArray();if(chars.Num()>1&&chars.Num()<1025&&chars.GetData()) {
                const auto path=to_string(StringType(chars.GetData(),chars.Num()-1));if(path.starts_with('/')) {
                    const auto normalized=PS::AssetAliases::Resolve(ActorHelper::NormalizeObjectPath(RC::to_generic_string(path)));
                    auto* linked=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,normalized.c_str(),false);if(!linked)linked=ActorHelper::ResolveObject(normalized);
                    if(linked)return ResolveDrawableObject(linked,depth+1,visited,next+"=>"+path);
                }
            }
        }
        if(auto* structure=CastField<FStructProperty>(property);structure&&structure->GetStruct()) {
            auto* type=structure->GetStruct().Get();static constexpr const char* preferred[]={"ResourceObject","Resource","Texture","Texture2D","Icon","Image","Brush","Asset","Object","Value"};
            for(const auto* name:preferred)if(auto* child=PropertyHelper::GetPropertyByName(type,to_wstring(name)))if(auto resolved=ResolveDrawableProperty(child,address,size,depth+1,visited,next);resolved.Texture)return resolved;
            std::size_t scanned=0;for(auto* child:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
                if(++scanned>48)break;if(!RelevantIconField(to_string(child->GetName())))continue;
                if(auto resolved=ResolveDrawableProperty(child,address,size,depth+1,visited,next);resolved.Texture)return resolved;
            }
        }
        return {};
    }
    ResolvedTexture ResolveDrawableObject(UObject* object,int depth,std::unordered_set<UObject*>& visited,const std::string& chain) {
        if(!object||depth>5||object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))||!visited.insert(object).second)return {};
        if(IsTexture(object))return {object,chain};
        auto* type=object->GetClassPrivate();if(!type)return {};
        static constexpr const char* preferred[]={"ResourceObject","Resource","Texture","Texture2D","Icon","ItemIcon","InventoryIcon","DisplayIcon","Image","Brush","Thumbnail","Asset","Object","Value"};
        const auto size=type->GetPropertiesSize();
        for(const auto* name:preferred)if(auto* property=PropertyHelper::GetPropertyByName(type,to_wstring(name)))if(auto resolved=ResolveDrawableProperty(property,object,size,depth+1,visited,chain);resolved.Texture)return resolved;
        std::size_t scanned=0;for(auto* property:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
            if(++scanned>96)break;if(!RelevantIconField(to_string(property->GetName())))continue;
            if(auto resolved=ResolveDrawableProperty(property,object,size,depth+1,visited,chain);resolved.Texture)return resolved;
        }
        return {};
    }
    void WriteThumbnail(const std::filesystem::path& file,const std::vector<unsigned char>& rgba) {
        if(rgba.size()!=ThumbnailSize*ThumbnailSize*4)throw std::runtime_error("Item icon thumbnail byte count is invalid");
        std::filesystem::create_directories(file.parent_path());
        const auto temporary=file.wstring()+L".tmp";
        std::ofstream output(std::filesystem::path(temporary),std::ios::binary|std::ios::trunc);
        if(!output)throw std::runtime_error("Cannot create item icon thumbnail cache file");
        const uint32_t width=ThumbnailSize,height=ThumbnailSize,bytes=static_cast<uint32_t>(rgba.size());
        output.write("RSI3",4);output.write(reinterpret_cast<const char*>(&width),sizeof(width));
        output.write(reinterpret_cast<const char*>(&height),sizeof(height));output.write(reinterpret_cast<const char*>(&bytes),sizeof(bytes));
        output.write(reinterpret_cast<const char*>(rgba.data()),static_cast<std::streamsize>(rgba.size()));output.close();
        if(!output)throw std::runtime_error("Item icon thumbnail write failed");
        std::error_code ec;std::filesystem::remove(file,ec);ec.clear();std::filesystem::rename(temporary,file,ec);
        if(ec)throw std::runtime_error("Item icon thumbnail could not be committed");
    }
}

int CacheLimitMB(){return ConfiguredCacheLimitMB();}
std::optional<nlohmann::json> Cached(const std::string& iconPath) {
    if(iconPath.empty()||iconPath.size()>1024||iconPath.front()!='/'||!HelpySourceCache::Ready())return {};
    const auto key=HelpySourceCache::Fingerprint(iconPath);if(key.empty())return {};
    const auto folder=HelpySourceCache::IconDirectory(iconPath,ConfiguredCacheLimitMB()==0);if(folder.empty())return {};
    const auto file=folder/(HelpySourceCache::Key(iconPath)+".rsicon");
    if(!CachedThumbnail(file))return {};
    return nlohmann::json{{"Icon",iconPath},{"File",file.string()},{"Width",ThumbnailSize},{"Height",ThumbnailSize},{"Status","Ready"},{"CacheHit",true},{"SourceKey",key}};
}

nlohmann::json Render(UWorld* world,const std::string& iconPath) {
    if(iconPath.empty()||iconPath.size()>1024||iconPath.front()!='/')throw std::runtime_error("Item icon request requires a canonical texture path");
    HelpySourceCache::Poll(HostServices::ModDirectory());if(!HelpySourceCache::Ready())return {{"Icon",iconPath},{"Status","Pending"}};
    if(HelpySourceCache::Current.Provisional){if(auto saved=Cached(iconPath))return *saved;return {{"Icon",iconPath},{"Status","Pending"}};}
    const auto cachedFile=OutputFile(iconPath);
    if(PendingWrite.valid()) {
        if(PendingWrite.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return {{"Icon",iconPath},{"Status","Pending"}};
        auto reply=std::move(PendingWriteReply);PendingWriteReply=nlohmann::json{};
        const bool matching=reply.value("Icon",std::string{})==iconPath&&reply.value("File",std::string{})==cachedFile.string();
        try{PendingWrite.get();}catch(...){if(matching)throw;}
        if(matching){QueueCacheMaintenance(cachedFile);return reply;}
    }
    if(PS::SlowWork::Measure("helpy.icons.cache-read",[&]{return CachedThumbnail(cachedFile);})) {ActiveCapture.reset();PS::HelpyIconTrace::Event("core-cache-hit",iconPath,"file='"+cachedFile.string()+"'");QueueCacheMaintenance(cachedFile);return {{"Icon",iconPath},{"File",cachedFile.string()},{"Width",ThumbnailSize},{"Height",ThumbnailSize},{"Status","Ready"},{"CacheHit",true},{"SourceKey",HelpySourceCache::Fingerprint(iconPath)}};}
    if(!world)throw std::runtime_error("Item icon rendering requires a loaded world");
    if(RendererWorld.Get()!=world){Reset();RendererWorld.Assign(world);}
    if(ActiveCapture&&(ActiveCapture->Icon!=iconPath||ActiveCapture->World.Get()!=world))ActiveCapture.reset();
    if(ActiveCapture) {
        auto& capture=*ActiveCapture;const auto now=CaptureNow();
        if(capture.Timing.Expired(now)){Reset();throw std::runtime_error("Item icon texture warm-up timed out");}
        auto* texture=capture.Texture.Get();
        if(!texture){Reset();throw std::runtime_error("Item icon capture texture expired");}
        auto* library=DefaultRenderingLibrary();
        const auto readCapture=[&]() -> nlohmann::json {
            auto* target=capture.Target.Get();if(!target){Reset();throw std::runtime_error("Item icon capture target expired");}
            ReflectedCall read(library,TEXT("/Script/Engine.KismetRenderingLibrary:ReadRenderTarget"));
            PS::SlowWork::Measure("helpy.icons.gpu-readback",[&]{read.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),target).JsonArg(TEXT("bNormalize"),false).Invoke();});
            if(!read.BoolReturn())throw std::runtime_error("ReadRenderTarget failed for item icon");
            auto pixels=ReadColors(read);std::size_t alpha=0,rgb=0;
            for(std::size_t i=0;i+3<pixels.size();i+=4){alpha+=pixels[i+3]!=0;rgb+=pixels[i]!=0||pixels[i+1]!=0||pixels[i+2]!=0;}
            // An opaque draw can turn an unready texture into a solid black
            // rectangle (nonzero alpha). Do not persist that as a valid icon.
            if((alpha==0&&rgb==0)||(capture.Timing.Opaque()&&rgb==0)) {
                // Keep the world-owned target alive. Recreating/releasing it
                // per icon reproduced "first icon only" on the native GPU path.
                // Clear before each draw; a retry must not recycle its RHI resource.
                if(!capture.Timing.Blank(now)){Reset();throw std::runtime_error("ReadRenderTarget produced a blank item icon after bounded warm-up");}
                PS::HelpyIconTrace::Event("capture-fallback",iconPath,"mode='retained-rt-opaque' reason='blank-readback'");
                return {{"Icon",iconPath},{"Status","Pending"}};
            }
            if(alpha==0)for(std::size_t i=0;i+3<pixels.size();i+=4)pixels[i+3]=std::max({pixels[i],pixels[i+1],pixels[i+2]});
            const auto result=nlohmann::json{{"Icon",iconPath},{"File",cachedFile.string()},{"Width",ThumbnailSize},{"Height",ThumbnailSize},{"Status","Ready"},{"CacheHit",false},{"SourceKey",HelpySourceCache::Fingerprint(iconPath)},{"ColdLoad",capture.Cold},{"ResolvedClass",capture.ResolvedClass},{"ResolvedChain",capture.ResolvedChain}};
            PendingWriteReply=result;
            // Only owned bytes/path cross this boundary; success is published after commit.
            PendingWrite=std::async(std::launch::async,[file=cachedFile,pixels=std::move(pixels)]{
                PS::SlowWork::Measure("helpy.icons.cache-write",[&]{WriteThumbnail(file,pixels);});
            });
            ActiveCapture.reset();return {{"Icon",iconPath},{"Status","Pending"}};
        };
        if(capture.Timing.CanRead(now))return readCapture();
        if(!capture.Timing.CanDraw(now))return {{"Icon",iconPath},{"Status","Pending"}};
        PS::SlowWork::Measure("helpy.icons.draw",[&]{
        auto* renderTarget=ReusableTarget(library,world);if(capture.Target.Get()!=renderTarget){capture.Target.Release();capture.Target.Hold(renderTarget);}
        ReflectedCall clear(library,TEXT("/Script/Engine.KismetRenderingLibrary:ClearRenderTarget2D"));
        clear.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),renderTarget)
            .JsonArg(TEXT("ClearColor"),{{"R",0.0},{"G",0.0},{"B",0.0},{"A",0.0}}).Invoke();
        ReflectedCall begin(library,TEXT("/Script/Engine.KismetRenderingLibrary:BeginDrawCanvasToRenderTarget"));
        begin.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),renderTarget).Invoke();
        auto* canvas=begin.ObjectValue(TEXT("Canvas"));if(!canvas)throw std::runtime_error("BeginDrawCanvasToRenderTarget returned no Canvas");
        try {
            ReflectedCall draw(canvas,TEXT("/Script/Engine.Canvas:K2_DrawTexture"));
            draw.ObjectArg(TEXT("RenderTexture"),texture)
                .JsonArg(TEXT("ScreenPosition"),{{"X",0.0},{"Y",0.0}}).JsonArg(TEXT("ScreenSize"),{{"X",ThumbnailSize},{"Y",ThumbnailSize}})
                .JsonArg(TEXT("CoordinatePosition"),{{"X",0.0},{"Y",0.0}}).JsonArg(TEXT("CoordinateSize"),{{"X",1.0},{"Y",1.0}})
                .JsonArg(TEXT("RenderColor"),{{"R",1.0},{"G",1.0},{"B",1.0},{"A",1.0}})
                .JsonArg(TEXT("BlendMode"),capture.Timing.Opaque()?"BLEND_Opaque":"BLEND_Translucent").JsonArg(TEXT("Rotation"),0.0).JsonArg(TEXT("PivotPoint"),{{"X",0.5},{"Y",0.5}}).Invoke();
        }catch(...) {
            try{ReflectedCall end(library,TEXT("/Script/Engine.KismetRenderingLibrary:EndDrawCanvasToRenderTarget"));end.ObjectArg(TEXT("WorldContextObject"),world).TakeDrawContext(begin).Invoke();}catch(...){}
            throw;
        }
        ReflectedCall end(library,TEXT("/Script/Engine.KismetRenderingLibrary:EndDrawCanvasToRenderTarget"));
        end.ObjectArg(TEXT("WorldContextObject"),world).TakeDrawContext(begin).Invoke();
        });
        capture.Timing.Submitted(now);
        // Helpy 2.0 completed EndDraw and ReadRenderTarget in the same call.
        // ReadRenderTarget synchronizes the submitted canvas work. Game Pass
        // has returned blank captures on the deferred-across-ticks path.
        // Retain bounded warm-up/retries, but never leave a submitted canvas
        // dependent on a later foreground game frame before its first read.
        return readCapture();
    }
    const auto normalized=PS::AssetAliases::Resolve(ActorHelper::NormalizeObjectPath(RC::to_generic_string(iconPath)));
    PS::HelpyIconTrace::Event("resolve-start",iconPath,"normalized='"+RC::to_string(normalized)+"'");
    auto* root=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,normalized.c_str(),false);
    const bool coldLoad=root==nullptr;PS::HelpyIconTrace::Event(root?"root-resident":"root-cold",iconPath);if(!root)root=PS::SlowWork::Measure("helpy.icons.asset-load",[&]{return ActorHelper::ResolveObject(normalized);});
    if(!root){PS::HelpyIconTrace::Event("root-load-fail",iconPath,"normalized='"+RC::to_string(normalized)+"'");throw std::runtime_error("stage=root-load: icon object could not be loaded: "+iconPath);}
    const auto rootClass=root->GetClassPrivate()?RC::to_string(root->GetClassPrivate()->GetPathName()):std::string("<unknown>");
    PS::HelpyIconTrace::Event("root-loaded",iconPath,"class='"+rootClass+"' object='"+RC::to_string(root->GetPathName())+"'");
    std::unordered_set<UObject*> visited;auto resolved=ResolveDrawableObject(root,0,visited,iconPath+" ["+rootClass+"]");auto* texture=resolved.Texture;
    if(!texture){PS::HelpyIconTrace::Event("unwrap-fail",iconPath,"rootClass='"+rootClass+"' visited="+std::to_string(visited.size()));throw std::runtime_error("stage=unwrap: no drawable Texture found under class="+rootClass+", path="+iconPath);}
    if(texture->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))){PS::HelpyIconTrace::Event("texture-state-fail",iconPath,"reason='destroying'");throw std::runtime_error("stage=texture-state: resolved texture is being destroyed: "+iconPath);}
    const auto resolvedClass=texture->GetClassPrivate()?RC::to_string(texture->GetClassPrivate()->GetPathName()):std::string("<unknown>");
    PS::HelpyIconTrace::Event("unwrap-ready",iconPath,"rootClass='"+rootClass+"' resolvedClass='"+resolvedClass+"' texture='"+RC::to_string(texture->GetPathName())+"' chain='"+resolved.Chain+"'");
    auto capture=std::make_unique<Capture>();
    capture->Icon=iconPath;capture->Cold=coldLoad;capture->ResolvedClass=resolvedClass;capture->ResolvedChain=resolved.Chain;
    capture->World.Assign(world);if(capture->World.Get()!=world)throw std::runtime_error("Cannot retain item icon world identity");
    capture->Texture.Hold(texture);capture->Timing.Begin(CaptureNow());ActiveCapture=std::move(capture);
    return {{"Icon",iconPath},{"Status","Pending"}};
}
void ShutdownDiskCache() noexcept {if(PendingWrite.valid())try{PendingWrite.get();}catch(...){}PendingWriteReply=nlohmann::json{};DiskMaintenance.Shutdown();}
void Reset() noexcept {
    ActiveCapture.reset();
    try{CaptureCleanup.Run([&]{if(auto* target=RendererTarget.Get())ReleaseRenderTarget(DefaultRenderingLibrary(),target);});}catch(...){}
    RendererTarget.Release();RendererWorld.Reset();
}
}
