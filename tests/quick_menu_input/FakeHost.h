#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Windows.h"
#include "Unreal/Hooks.hpp"

namespace RC {
inline std::wstring to_generic_string(const char* s){return {s,s+std::strlen(s)};}
namespace Unreal {
enum EObjectFlags {RF_ClassDefaultObject=1,RF_ArchetypeObject=2,RF_BeginDestroyed=4,RF_FinishDestroyed=8};
constexpr unsigned CPF_Parm=1,CPF_OutParm=2,CPF_ReturnParm=4;
constexpr unsigned FUNC_Native=1,FUNC_Static=2;
struct MockFieldClass { std::wstring name; std::wstring GetName() { return name; } };
inline bool fakeObjectVirtualsAvailable = false;
inline int fakeObjectVirtualCalls = 0;
constexpr unsigned CPF_ReferenceParm=0x1000, CPF_UObjectWrapper=0x2000;
struct FProperty {
    virtual ~FProperty()=default;
    int offset=0,size=1,dim=1;unsigned flags=0;
    int initialized=0,destroyed=0,failInitOn=0;bool failInit=false;
    int GetArrayDim(){return dim;}int GetElementSize(){return size;}int GetOffset_Internal(){return offset;}
    bool HasAnyPropertyFlags(unsigned f){return (flags&f)!=0;}
    void InitializeValue_InContainer(void* data){if(failInit || (failInitOn && initialized+1==failInitOn))throw std::runtime_error("init failure");++initialized;std::memset(static_cast<std::uint8_t*>(data)+offset,0,size);}
    void DestroyValue_InContainer(void*){++destroyed;}
    template<class T>T* ContainerPtrToValuePtr(void* data){return reinterpret_cast<T*>(static_cast<std::uint8_t*>(data)+offset);}
};
struct FBoolProperty: FProperty {
    std::uint8_t mask=1;bool skipWrite=false;
    bool GetPropertyValue(void* p){return (*static_cast<std::uint8_t*>(p)&mask)!=0;}
    void SetPropertyValue(void* p,bool v){if(skipWrite)return;auto& b=*static_cast<std::uint8_t*>(p);b=v?std::uint8_t(b|mask):std::uint8_t(b&~mask);}
};
template<class T> T* CastField(FProperty* p){return dynamic_cast<T*>(p);}
struct UClass {int extent=2048;UClass* parent=nullptr;std::map<std::wstring,FProperty*> fields;int GetPropertiesSize(){return extent;}};
template<class T>struct Ptr {T* value=nullptr;T* Get(){return value;}};
constexpr int FNAME_Find=0;
struct FName {std::wstring value;FName(const wchar_t* s,int=0):value(s){};std::wstring ToString()const{return value;}};
struct UEnum {struct Pair{FName Key;std::int64_t Value;};std::vector<Pair> names;const auto& GetEnumNames(){return names;}};
struct FNumericProperty:FProperty {
    UEnum* enumeration=nullptr;bool integer=true,skipWrite=false;
    bool IsInteger(){return integer;}UEnum* GetIntPropertyEnum(){return enumeration;}
    std::int64_t GetSignedIntPropertyValue(void* p){return *static_cast<std::uint8_t*>(p);}
    void SetIntPropertyValue(void* p,std::uint64_t v){if(!skipWrite)*static_cast<std::uint8_t*>(p)=static_cast<std::uint8_t>(v);}
};
struct FEnumProperty:FProperty {UEnum* enumeration=nullptr;FNumericProperty number;
    UEnum* GetEnum(){return enumeration;}FNumericProperty* GetUnderlyingProperty(){return &number;}};
struct FObjectPropertyBase:FProperty {Ptr<UClass> propertyClass;bool skipWrite=false;
    MockFieldClass fieldClass{L"ObjectPropertyBase"};
    MockFieldClass GetClass(){return fieldClass;}
    FObjectPropertyBase(){size=sizeof(void*);}Ptr<UClass> GetPropertyClass(){return propertyClass;}
    void SetObjectPropertyValue(void* p,UObject* value){++fakeObjectVirtualCalls;
        if(!fakeObjectVirtualsAvailable)throw std::runtime_error("Virtual FObjectPropertyBase::SetObjectPropertyValue is unavailable, possibly unsupported in engine version");
        if(!skipWrite)std::memcpy(p,&value,sizeof(value));}
    UObject* GetObjectPropertyValue(void* p){++fakeObjectVirtualCalls;
        if(!fakeObjectVirtualsAvailable)throw std::runtime_error("Virtual FObjectPropertyBase::GetObjectPropertyValue is unavailable");
        UObject* value=nullptr;std::memcpy(&value,p,sizeof(value));return value;}
};
struct FObjectProperty:FObjectPropertyBase {FObjectProperty(){fieldClass.name=L"ObjectProperty";}};
class UFunction {
public:
    enum Kind {Look,Move,LookQuery,MoveQuery,UIOnly,GameOnly,GameAndUI,Draw} kind=Draw;
    int parms=1;unsigned flags=FUNC_Native|FUNC_Static;bool failBefore=false,failAfter=false;
    std::map<std::wstring,FProperty*> fields;
    int GetParmsSize(){return parms;}unsigned GetFunctionFlags(){return flags;}
    FProperty* FindProperty(const FName& n){auto p=fields.find(n.value);return p==fields.end()?nullptr:p->second;}
    FProperty* GetReturnProperty(){for(auto& [_,p]:fields)if(p->HasAnyPropertyFlags(CPF_ReturnParm))return p;return nullptr;}
};
enum class EFieldIterationFlags { Default };
template<class T>struct TFieldRange {
    std::vector<T*> v;TFieldRange(UFunction* f,EFieldIterationFlags){for(auto& [_,p]:f->fields)v.push_back(static_cast<T*>(p));}
    auto begin(){return v.begin();}auto end(){return v.end();}
};
class UObject {
public:
    std::array<std::uint8_t,2048> storage{};
    UClass* type=nullptr;UObject* world=nullptr;UObject* pawn=nullptr;unsigned flags=0;
    int serial=1,lookCount=0,moveCount=0,events=0,uiCalls=0,gameCalls=0,flushes=0,inputMode=0;
    bool failWeak=false,throwLook=false,throwMove=false;
    UClass* GetClassPrivate(){return type;}
    bool HasAnyFlags(EObjectFlags f){return (flags&static_cast<unsigned>(f))!=0;}
    bool IsA(UClass* t){for(auto* c=type;c;c=c->parent)if(c==t)return true;return false;}
    UObject* GetWorld(){return world;}
    void ProcessEvent(UFunction* f,void* data){
        Hook::TCallbackIterationData<void> iteration;
        for(auto& [_,callback]:Hook::eventCallbacks)callback(iteration,this,f,data);
        if(f->failBefore || (f->kind==UFunction::Look&&throwLook) || (f->kind==UFunction::Move&&throwMove))throw std::runtime_error("injected dispatch failure");
        ++events;
        if(f->kind==UFunction::LookQuery||f->kind==UFunction::MoveQuery){auto* p=CastField<FBoolProperty>(f->GetReturnProperty());p->SetPropertyValue(p->ContainerPtrToValuePtr<void>(data),(f->kind==UFunction::LookQuery?lookCount:moveCount)>0);}
        else if(f->kind==UFunction::Look||f->kind==UFunction::Move){auto* p=CastField<FBoolProperty>(f->fields.begin()->second);bool on=p->GetPropertyValue(p->ContainerPtrToValuePtr<void>(data));if(f->kind==UFunction::Look)lookCount+=on?1:-1;else moveCount+=on?1:-1;}
        else if(f->kind==UFunction::UIOnly||f->kind==UFunction::GameOnly||f->kind==UFunction::GameAndUI){
            auto* p=CastField<FObjectPropertyBase>(f->fields.at(L"PlayerController"));UObject* pc=nullptr;std::memcpy(&pc,p->ContainerPtrToValuePtr<void>(data),sizeof(pc));
            if(!pc)throw std::runtime_error("null controller arg");
            auto* flush=CastField<FBoolProperty>(f->fields.at(L"bFlushInput"));if(flush->GetPropertyValue(flush->ContainerPtrToValuePtr<void>(data)))++pc->flushes;
            if(f->kind==UFunction::GameOnly){++pc->gameCalls;pc->inputMode=0;}else{++pc->uiCalls;pc->inputMode=f->kind==UFunction::UIOnly?1:2;}
        }
        if(f->failAfter)throw std::runtime_error("injected post-dispatch failure");
    }
};
}}
namespace DragonWilds {
namespace ActorHelper {
inline std::map<std::wstring,RC::Unreal::UClass*> classes;
inline RC::Unreal::UClass* ResolveClass(const TCHAR* path){auto p=classes.find(path);return p==classes.end()?nullptr:p->second;}
inline RC::Unreal::UObject* GetObjectRef(RC::Unreal::UObject* o,const TCHAR*){return o?o->pawn:nullptr;}
}
namespace PropertyHelper {
inline RC::Unreal::FProperty* GetPropertyByName(RC::Unreal::UClass* c,const TCHAR* n){auto p=c->fields.find(n);return p==c->fields.end()?nullptr:p->second;}
}
}
namespace UECustom::UObjectGlobals {
inline RC::Unreal::UObject* library=nullptr;
template<class T>T StaticFindObject(void*,void*,const wchar_t*,bool){return static_cast<T>(library);}
}
namespace PS {
enum class LogLevel {Normal,Warning,Verbose};inline std::vector<std::wstring> fakeLogs;
template<class T>std::wstring MockLogValue(const T& value){std::wostringstream out;out<<value;return out.str();}
inline bool fakeLogThrows=false;
template<LogLevel L,class...Args>void Log(const wchar_t* format,Args&&...args){
    if(fakeLogThrows)throw std::runtime_error("injected logger failure");
    const std::array<std::wstring,sizeof...(Args)> values{MockLogValue(args)...};
    std::wstring message=format;std::size_t from=0;
    for(const auto& value:values){const auto at=message.find(L"{}",from);if(at==std::wstring::npos)break;
        message.replace(at,2,value);from=at+value.size();}
    const wchar_t* level=L==LogLevel::Normal?L"[normal] ":L==LogLevel::Warning?L"[warning] ":L"[verbose] ";
    fakeLogs.emplace_back(std::wstring(level)+message);
}
namespace SpawnToolRequests {inline std::atomic<bool> CancelRequested{false};}
}
inline std::vector<RC::Unreal::UObject*> fakeObjects;
inline DWORD fakeProtect=PAGE_READWRITE;inline bool fakeQueryFails=false;
inline std::size_t VirtualQuery(void* address,MEMORY_BASIC_INFORMATION* out,std::size_t){
    if(fakeQueryFails)return 0;
    for(auto* o:fakeObjects){auto a=reinterpret_cast<std::uintptr_t>(address),b=reinterpret_cast<std::uintptr_t>(o);
        if(a>=b&&a-b<sizeof(*o)){*out={o,sizeof(*o),MEM_COMMIT,fakeProtect};return sizeof(*out);}}
    return 0;
}
inline bool GetModuleHandleExW(DWORD,LPCWSTR,HMODULE* module){*module=reinterpret_cast<HMODULE>(std::uintptr_t{44});return true;}
inline DWORD GetModuleFileNameW(HMODULE,wchar_t* path,DWORD size){const wchar_t* value=L"C:\\game\\ue4ss\\Mods\\RuneSchema\\dlls\\main.dll";if(size<64)return 0;std::wcscpy(path,value);return static_cast<DWORD>(std::wcslen(value));}
