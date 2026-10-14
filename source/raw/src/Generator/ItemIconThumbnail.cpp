#include "Generator/ItemIconThumbnail.h"
#include "Generator/HelpyIconTrace.h"
#include "Generator/HelpyRuntimePreferences.h"
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"
#include "Utility/Logging.h"
#include "Utility/AssetAliases.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
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

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;

namespace PS::ItemIconThumbnail {
namespace {
    constexpr int ThumbnailSize=64;
    constexpr std::uintmax_t LiveSessionCacheLimitBytes=32ull*1024ull*1024ull;
    bool CacheBytesKnown=false;
    std::uintmax_t CacheBytes=0;


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
        static UObject* cached=nullptr;
        if(cached&&!cached->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return cached;
        cached=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetRenderingLibrary"),false);
        if(!cached)throw std::runtime_error("KismetRenderingLibrary is unavailable");return cached;
    }

    int ConfiguredCacheLimitMB() {
        static int cached=200;static bool haveStamp=false;static std::filesystem::file_time_type stamp{};static std::chrono::steady_clock::time_point next{};
        const auto now=std::chrono::steady_clock::now();if(now<next)return cached;next=now+std::chrono::milliseconds(500);
        const auto file=HostServices::WorkingDirectory()/"Mods"/"RuneSchema"/"plugins"/"RuneSchema.Helpy"/"settings"/"ui.jsonc";std::error_code ec;
        const auto currentStamp=std::filesystem::last_write_time(file,ec);if(ec)return cached;if(haveStamp&&currentStamp==stamp)return cached;
        try {const auto doc=nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true);const auto c=doc.value("cache",nlohmann::json::object());cached=std::clamp(c.value("iconCacheMB",200),0,16384);stamp=currentStamp;haveStamp=true;}catch(...){}
        return cached;
    }

    uint64_t HashPath(const std::string& value) {
        uint64_t hash=1469598103934665603ull;for(unsigned char byte:value){hash^=byte;hash*=1099511628211ull;}return hash;
    }

    std::filesystem::path OutputFile(const std::string& iconPath) {
        const bool liveOnly=ConfiguredCacheLimitMB()==0;
        auto folder=HostServices::CacheDirectory()/(liveOnly?"item-icons-live":"item-icons");
        static std::uint64_t lastCacheEpoch=HelpyRuntimePreferences::IconCacheEpoch.load();const auto cacheEpoch=HelpyRuntimePreferences::IconCacheEpoch.load();if(cacheEpoch!=lastCacheEpoch){CacheBytesKnown=false;CacheBytes=0;lastCacheEpoch=cacheEpoch;}
        static int lastMode=-1;const int mode=liveOnly?0:1;
        if(liveOnly&&lastMode!=0){std::error_code ec;std::filesystem::remove_all(folder,ec);CacheBytesKnown=false;CacheBytes=0;}
        if(lastMode!=mode){CacheBytesKnown=false;CacheBytes=0;lastMode=mode;}
        std::filesystem::create_directories(folder);
        return folder/(std::format("{:016x}.rsicon",HashPath(iconPath)));
    }
    bool CachedThumbnail(const std::filesystem::path& file) {
        std::error_code ec;if(!std::filesystem::is_regular_file(file,ec)||ec)return false;
        const auto size=std::filesystem::file_size(file,ec);if(ec||size!=16u+ThumbnailSize*ThumbnailSize*4u)return false;
        std::ifstream input(file,std::ios::binary);if(!input)return false;char magic[4]{};uint32_t w=0,h=0,bytes=0;
        input.read(magic,4);input.read(reinterpret_cast<char*>(&w),sizeof(w));input.read(reinterpret_cast<char*>(&h),sizeof(h));input.read(reinterpret_cast<char*>(&bytes),sizeof(bytes));
        if(!input||std::string(magic,4)!="RSI1"||w!=ThumbnailSize||h!=ThumbnailSize||bytes!=ThumbnailSize*ThumbnailSize*4u)return false;
        std::vector<unsigned char> pixels(bytes);input.read(reinterpret_cast<char*>(pixels.data()),bytes);if(!input)return false;
        std::size_t alphaNonZero=0,rgbNonZero=0;
        for(std::size_t i=0;i+3<pixels.size();i+=4){if(pixels[i+3]!=0)++alphaNonZero;if(pixels[i]!=0||pixels[i+1]!=0||pixels[i+2]!=0)++rgbNonZero;}
        return alphaNonZero>0||rgbNonZero>0;
    }
    void MaintainCache(const std::filesystem::path& folder,std::uintmax_t addedBytes=0) {
        struct Entry{std::filesystem::path File;std::filesystem::file_time_type Used;std::uintmax_t Size;};
        std::vector<Entry> files;std::error_code ec;
        if(!CacheBytesKnown) {
            CacheBytes=0;
            for(std::filesystem::directory_iterator it(folder,ec),end;!ec&&it!=end;it.increment(ec)) {
                if(!it->is_regular_file(ec)||ec||it->path().extension()!=L".rsicon"){ec.clear();continue;}
                const auto size=it->file_size(ec);if(ec){ec.clear();continue;}CacheBytes+=size;
            }
            CacheBytesKnown=true;
        } else CacheBytes+=addedBytes;
        const int configuredMB=ConfiguredCacheLimitMB();
        const std::uintmax_t limitBytes=configuredMB<=0?LiveSessionCacheLimitBytes:static_cast<std::uintmax_t>(configuredMB)*1024ull*1024ull;
        const std::uintmax_t trimTarget=configuredMB<=0?(LiveSessionCacheLimitBytes*3ull/4ull):(limitBytes*95ull/100ull);
        if(CacheBytes<=limitBytes)return;
        std::uintmax_t total=0;ec.clear();
        for(std::filesystem::directory_iterator it(folder,ec),end;!ec&&it!=end;it.increment(ec)) {
            if(!it->is_regular_file(ec)||ec||it->path().extension()!=L".rsicon"){ec.clear();continue;}
            const auto size=it->file_size(ec);if(ec){ec.clear();continue;}const auto used=it->last_write_time(ec);if(ec){ec.clear();continue;}
            files.push_back({it->path(),used,size});total+=size;
        }
        std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.Used<b.Used;});
        for(const auto& entry:files){if(total<=trimTarget)break;std::filesystem::remove(entry.File,ec);if(!ec)total-=std::min(total,entry.Size);else ec.clear();}
        CacheBytes=total;
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
    if(iconPath.empty()||iconPath.size()>1024||iconPath.front()!='/')throw std::runtime_error("Item icon request requires a canonical texture path");
    const auto cachedFile=OutputFile(iconPath);if(CachedThumbnail(cachedFile)) {PS::HelpyIconTrace::Event("core-cache-hit",iconPath,"file='"+cachedFile.string()+"'");std::error_code touch;std::filesystem::last_write_time(cachedFile,std::filesystem::file_time_type::clock::now(),touch);MaintainCache(cachedFile.parent_path());return {{"Icon",iconPath},{"File",cachedFile.string()},{"Width",ThumbnailSize},{"Height",ThumbnailSize},{"Status","Ready"},{"CacheHit",true}};}
    if(!world)throw std::runtime_error("Item icon rendering requires a loaded world");
    const auto normalized=PS::AssetAliases::Resolve(ActorHelper::NormalizeObjectPath(RC::to_generic_string(iconPath)));
    PS::HelpyIconTrace::Event("resolve-start",iconPath,"normalized='"+RC::to_string(normalized)+"'");
    auto* root=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,normalized.c_str(),false);
    const bool coldLoad=root==nullptr;PS::HelpyIconTrace::Event(root?"root-resident":"root-cold",iconPath);if(!root)root=ActorHelper::ResolveObject(normalized);
    if(!root){PS::HelpyIconTrace::Event("root-load-fail",iconPath,"normalized='"+RC::to_string(normalized)+"'");throw std::runtime_error("stage=root-load: icon object could not be loaded: "+iconPath);}
    const auto rootClass=root->GetClassPrivate()?RC::to_string(root->GetClassPrivate()->GetPathName()):std::string("<unknown>");
    PS::HelpyIconTrace::Event("root-loaded",iconPath,"class='"+rootClass+"' object='"+RC::to_string(root->GetPathName())+"'");
    std::unordered_set<UObject*> visited;auto resolved=ResolveDrawableObject(root,0,visited,iconPath+" ["+rootClass+"]");auto* texture=resolved.Texture;
    if(!texture){PS::HelpyIconTrace::Event("unwrap-fail",iconPath,"rootClass='"+rootClass+"' visited="+std::to_string(visited.size()));throw std::runtime_error("stage=unwrap: no drawable Texture found under class="+rootClass+", path="+iconPath);}
    if(texture->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))){PS::HelpyIconTrace::Event("texture-state-fail",iconPath,"reason='destroying'");throw std::runtime_error("stage=texture-state: resolved texture is being destroyed: "+iconPath);}
    const auto resolvedClass=texture->GetClassPrivate()?RC::to_string(texture->GetClassPrivate()->GetPathName()):std::string("<unknown>");
    PS::HelpyIconTrace::Event("unwrap-ready",iconPath,"rootClass='"+rootClass+"' resolvedClass='"+resolvedClass+"' texture='"+RC::to_string(texture->GetPathName())+"' chain='"+resolved.Chain+"'");
    auto* library=DefaultRenderingLibrary();UObject* renderTarget=nullptr;
    try {
        renderTarget=CreateThumbnailRenderTarget(library,world);PS::HelpyIconTrace::Event("render-target",iconPath,"created=true");

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

        auto readPixels=[&](UObject* target,const char* stage){
            ReflectedCall read(library,TEXT("/Script/Engine.KismetRenderingLibrary:ReadRenderTarget"));
            read.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),target).JsonArg(TEXT("bNormalize"),false).Invoke();
            if(!read.BoolReturn()){PS::HelpyIconTrace::Event("readback-fail",iconPath,std::string("stage='")+stage+"' reason='ReadRenderTarget=false'");throw std::runtime_error("ReadRenderTarget failed for item icon");}
            auto pixels=ReadColors(read);
            std::size_t alphaNonZero=0,rgbNonZero=0;for(std::size_t i=0;i+3<pixels.size();i+=4){if(pixels[i+3]!=0)++alphaNonZero;if(pixels[i]!=0||pixels[i+1]!=0||pixels[i+2]!=0)++rgbNonZero;}
            bool alphaRecovered=false;if(alphaNonZero==0&&rgbNonZero>0){for(std::size_t i=0;i+3<pixels.size();i+=4)pixels[i+3]=std::max({pixels[i],pixels[i+1],pixels[i+2]});alphaRecovered=true;alphaNonZero=rgbNonZero;}
            PS::HelpyIconTrace::Event("readback-ready",iconPath,std::string("stage='")+stage+"' bytes="+std::to_string(pixels.size())+" alphaNonZero="+std::to_string(alphaNonZero)+" rgbNonZero="+std::to_string(rgbNonZero)+" alphaRecovered="+(alphaRecovered?std::string("true"):std::string("false")));
            return std::tuple<std::vector<unsigned char>,std::size_t,std::size_t>(std::move(pixels),alphaNonZero,rgbNonZero);
        };
        auto [rgba,alphaNonZero,rgbNonZero]=readPixels(renderTarget,"primary");
        if(alphaNonZero==0&&rgbNonZero==0){
            PS::HelpyIconTrace::Event("capture-fallback",iconPath,"reason='blank-primary' mode='fresh-rt-opaque'");
            ReleaseRenderTarget(library,renderTarget);renderTarget=CreateThumbnailRenderTarget(library,world);
            ReflectedCall clear2(library,TEXT("/Script/Engine.KismetRenderingLibrary:ClearRenderTarget2D"));
            clear2.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),renderTarget)
                .JsonArg(TEXT("ClearColor"),{{"R",0.0},{"G",0.0},{"B",0.0},{"A",0.0}}).Invoke();
            ReflectedCall begin2(library,TEXT("/Script/Engine.KismetRenderingLibrary:BeginDrawCanvasToRenderTarget"));
            begin2.ObjectArg(TEXT("WorldContextObject"),world).ObjectArg(TEXT("TextureRenderTarget"),renderTarget).Invoke();
            auto* canvas2=begin2.ObjectValue(TEXT("Canvas"));if(!canvas2)throw std::runtime_error("Fallback BeginDrawCanvasToRenderTarget returned no Canvas");
            bool ended2=false;
            try {
                ReflectedCall draw2(canvas2,TEXT("/Script/Engine.Canvas:K2_DrawTexture"));
                draw2.ObjectArg(TEXT("RenderTexture"),texture)
                    .JsonArg(TEXT("ScreenPosition"),{{"X",0.0},{"Y",0.0}})
                    .JsonArg(TEXT("ScreenSize"),{{"X",ThumbnailSize},{"Y",ThumbnailSize}})
                    .JsonArg(TEXT("CoordinatePosition"),{{"X",0.0},{"Y",0.0}})
                    .JsonArg(TEXT("CoordinateSize"),{{"X",1.0},{"Y",1.0}})
                    .JsonArg(TEXT("RenderColor"),{{"R",1.0},{"G",1.0},{"B",1.0},{"A",1.0}})
                    .JsonArg(TEXT("BlendMode"),"BLEND_Opaque").JsonArg(TEXT("Rotation"),0.0)
                    .JsonArg(TEXT("PivotPoint"),{{"X",0.5},{"Y",0.5}}).Invoke();
                ReflectedCall end2(library,TEXT("/Script/Engine.KismetRenderingLibrary:EndDrawCanvasToRenderTarget"));
                end2.ObjectArg(TEXT("WorldContextObject"),world).CopyArg(TEXT("Context"),begin2,TEXT("Context")).Invoke();ended2=true;
            } catch(...) {
                if(!ended2)try {ReflectedCall end2(library,TEXT("/Script/Engine.KismetRenderingLibrary:EndDrawCanvasToRenderTarget"));end2.ObjectArg(TEXT("WorldContextObject"),world).CopyArg(TEXT("Context"),begin2,TEXT("Context")).Invoke();}catch(...){}
                throw;
            }
            auto fallback=readPixels(renderTarget,"fallback-opaque");rgba=std::move(std::get<0>(fallback));alphaNonZero=std::get<1>(fallback);rgbNonZero=std::get<2>(fallback);
            if(alphaNonZero!=0||rgbNonZero!=0)PS::HelpyIconTrace::Event("capture-fallback-ready",iconPath,"mode='fresh-rt-opaque'");
        }
        if(alphaNonZero==0&&rgbNonZero==0){PS::HelpyIconTrace::Event("readback-fail",iconPath,"reason='blank-payload-after-fallback'");throw std::runtime_error("ReadRenderTarget produced a blank item icon");}
        WriteThumbnail(cachedFile,rgba);PS::HelpyIconTrace::Event("cache-write",iconPath,"file='"+cachedFile.string()+"'");std::error_code cacheSizeError;const auto cachedBytes=std::filesystem::file_size(cachedFile,cacheSizeError);MaintainCache(cachedFile.parent_path(),cacheSizeError?0:cachedBytes);ReleaseRenderTarget(library,renderTarget);renderTarget=nullptr;
        return {{"Icon",iconPath},{"File",cachedFile.string()},{"Width",ThumbnailSize},{"Height",ThumbnailSize},{"Status","Ready"},{"CacheHit",false},{"ColdLoad",coldLoad},{"ResolvedClass",resolvedClass},{"ResolvedChain",resolved.Chain}};
    } catch(const std::exception& error) {PS::HelpyIconTrace::Event("render-exception",iconPath,"error='"+std::string(error.what())+"'");ReleaseRenderTarget(library,renderTarget);throw;} catch(...) {PS::HelpyIconTrace::Event("render-exception",iconPath,"error='<non-std exception>'");ReleaseRenderTarget(library,renderTarget);throw;}
}
}
