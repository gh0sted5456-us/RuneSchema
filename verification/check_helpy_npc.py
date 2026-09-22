"""Compile shipped NPC guards and four actual owned-lease methods with engine stand-ins.
No real Unreal reflection, SPUD, vendor UI, Windows code or actor destruction runs here.
"""
from pathlib import Path
import argparse, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--sanitize',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1]
source=(root/'raw/src/Loader/HelpyNpcAuthoring.inl').read_text()
start=source.index('AActor* DragonWildsNpcLoader::FindHelpyNpc(')
end=source.index('nlohmann::json DragonWildsNpcLoader::SpawnHelpyNpc(')
methods=source[start:end]
mock=r'''
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#define TEXT(s) L##s
namespace RC::Unreal {
using EObjectFlags=unsigned;
inline constexpr EObjectFlags RF_Transient=1,RF_ClassDefaultObject=2,RF_ArchetypeObject=4;
struct UClass;struct UWorld;
struct UObject {
 UClass* type=nullptr;UWorld* world=nullptr;bool usable=true,throwUsable=false;unsigned version=1;
 virtual ~UObject()=default;
 UWorld* GetWorld()const{return world;}
 bool IsA(UClass*)const;
};
struct FProperty {
 int dim=1,offset=0,size=1;virtual ~FProperty()=default;
 int GetArrayDim()const{return dim;}int GetOffset_Internal()const{return offset;}int GetSize()const{return size;}
 template<class T>T* ContainerPtrToValuePtr(void* p)const;
};
struct FBoolProperty:FProperty {
 void SetPropertyValue(void* p,bool b){*static_cast<unsigned char*>(p)=b?1:0;}
 bool GetPropertyValue(void* p)const{return *static_cast<unsigned char*>(p)!=0;}
};
struct StructType {std::wstring path=L"/Script/CoreUObject.Guid";std::wstring GetPathName()const{return path;}};
struct FStructProperty:FProperty {StructType value;StructType* structure=&value;StructType* GetStruct()const{return structure;}};
struct UClass {
 int size=64;bool npc=true,abstract=false;FProperty *guid=nullptr,*skip=nullptr;
 int GetPropertiesSize()const{return size;}bool IsChildOf(UClass* base)const{return this==base||(npc&&base&&base->npc);}
};
inline bool UObject::IsA(UClass* c)const{return type&&type->IsChildOf(c);}
struct UWorld:UObject {};
struct AActor:UObject {
 std::array<unsigned char,64> bytes{};EObjectFlags flags=0;int requests=0;bool destroyFails=false;
 UClass* GetClassPrivate()const{return type;}bool HasAnyFlags(EObjectFlags f)const{return (flags&f)!=0;}
 void SetFlags(EObjectFlags f){flags|=f;}
};
template<class T>T* FProperty::ContainerPtrToValuePtr(void* p)const{return reinterpret_cast<T*>(static_cast<AActor*>(p)->bytes.data()+offset);}
struct UFunction {};
}
namespace DragonWilds {
namespace PropertyHelper {
using namespace RC::Unreal;
inline FProperty* GetPropertyByName(UClass* c,const wchar_t* n){return !c?nullptr:std::wstring(n)==L"SpudGuid"?c->guid:c->skip;}
template<class T>T* CastProperty(FProperty* f){return dynamic_cast<T*>(f);}
}
namespace ActorHelper {
using namespace RC::Unreal;
inline UClass base;
inline UClass* ResolveClass(const wchar_t*){return &base;}inline bool IsAbstract(UClass* c){return !c||c->abstract;}
struct FunctionCall {FunctionCall(AActor*,const wchar_t*){}template<class T>FunctionCall& Arg(const wchar_t*,T){return *this;}void Invoke(){}};
inline void DestroyActor(AActor* a){++a->requests;if(a->destroyFails)throw std::runtime_error("native destroy failed");/* deliberately latent */}
}
}
namespace PS {
struct WeakObjectHandle {
 RC::Unreal::UObject* token=nullptr;unsigned serial=0;
 WeakObjectHandle()=default;explicit WeakObjectHandle(RC::Unreal::UObject* o):token(o),serial(o?o->version:0){}
 RC::Unreal::UObject* Get()const{return token&&token->version==serial?token:nullptr;}
};
inline std::wstring ToWideSafe(const char* s){return std::wstring(s,s+std::strlen(s));}
}
'''
program=r'''
#include "Generator/HelpyNodes.h"
#include "Loader/HelpyNpcGuards.h"
#include <iostream>
namespace DragonWilds {
using namespace RC::Unreal;
static double clockNow=100;
static double HelpyNpcClock(){return clockNow;}
static bool IsNpcObjectUsable(UObject* o){if(o&&o->throwUsable)throw 17;return o&&o->usable;}
class DragonWildsNpcLoader {
public:
 struct VendorDefinition{std::string ModName="M",Id="owned";bool HelpyTemporary=true,Enabled=true;UClass* BaseActorClass=nullptr;};
 struct HelpyNpcLease{PS::WeakObjectHandle Actor,World;std::string Key;PS::HelpyNodes::Lease Lifetime;std::string AppliedKey{};std::wstring ObjectPath{};};
 struct Binding{std::string DefinitionKey;};struct Session{std::string NpcKey;};struct Spawned{std::string Key;};
 std::vector<VendorDefinition> m_definitions;
 std::vector<HelpyNpcLease> m_helpyNpcs;
 std::vector<Binding> m_merchantBindings;
 std::map<std::string,std::unique_ptr<Session>> m_dialogueSessions;
 std::vector<Spawned> m_spawnedVendors;
 std::set<std::string> m_applied;
 std::map<std::wstring,int> m_npcNames;
 std::set<AActor*> pendingCleanup;unsigned errors=0;
 std::string ActorKey(AActor*,const VendorDefinition& d){return d.ModName+":"+d.Id;}
 void QueueNpcCleanup(AActor* a){pendingCleanup.insert(a);}
 void ErrorOnce(const std::string&,const std::wstring&){++errors;}
 AActor* FindHelpyNpc(UWorld*,const VendorDefinition&)const;
 void RetireHelpyNpc(HelpyNpcLease&);
 void PumpHelpyNpcs();std::size_t DismissHelpyNpcs(UWorld* world=nullptr);
};
'''+methods+r'''
}
using namespace RC::Unreal;using namespace DragonWilds;
unsigned tested=0;
void check(bool c,const char* text){++tested;if(!c)throw std::runtime_error(text);}
template<class F>void reject(F f,const char* text){bool caught=false;try{f();}catch(...){caught=true;}check(caught,text);}
struct Fixture {
 UClass type;UWorld world,other;AActor actor,unrelated;FStructProperty guid;FBoolProperty skip;
 Fixture(){guid.offset=16;guid.size=16;type.guid=&guid;type.skip=&skip;actor.type=unrelated.type=&type;actor.world=unrelated.world=&world;actor.bytes.fill(7);}
};
DragonWildsNpcLoader loader(Fixture& f){
 DragonWildsNpcLoader l;HelpyNpcGuards::Exclude(&f.actor);
 l.m_definitions.push_back({"M","owned",true,true,&f.type});l.m_definitions.push_back({"M","source",false,true,&f.type});
 l.m_helpyNpcs.push_back({PS::WeakObjectHandle(&f.actor),PS::WeakObjectHandle(&f.world),"M:owned",PS::HelpyNodes::Lease::Start(100,30),"M:owned",L"/World.Owned"});
 l.m_merchantBindings={{"M:owned"},{"M:source"}};l.m_spawnedVendors={{"M:owned"},{"M:source"}};l.m_applied={"M:owned","M:source"};l.m_npcNames={{L"/World.Owned",1},{L"/World.Owned:Interaction",1},{L"/World.Source",1}};return l;
}
int main(){try {
 Fixture f;HelpyNpcGuards::TemporaryClass(&f.type);HelpyNpcGuards::Exclude(&f.actor);HelpyNpcGuards::VerifyExcluded(&f.actor);
 check(f.actor.HasAnyFlags(RF_Transient),"runtime flag set only on new actor");check(f.actor.bytes[0]==1,"save-exclusion flag set");
 for(int i=16;i<32;++i)check(f.actor.bytes[i]==0,"native GUID cleared");
 check(f.actor.bytes[2]==7&&f.actor.bytes[32]==7,"unrelated bytes retained");
 reject([&]{HelpyNpcGuards::Exclude(nullptr);},"null actor blocked");
 f.actor.flags|=RF_ClassDefaultObject;reject([&]{HelpyNpcGuards::Exclude(&f.actor);},"class default never edited");f.actor.flags=0;
 f.actor.flags|=RF_ArchetypeObject;reject([&]{HelpyNpcGuards::Exclude(&f.actor);},"archetype never edited");f.actor.flags=0;
 f.type.abstract=true;reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"abstract NPC blocked");f.type.abstract=false;
 f.type.npc=false;reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"non-NPC blocked");f.type.npc=true;
 f.type.skip=nullptr;reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"missing skip contract blocked");f.type.skip=&f.skip;
 f.type.guid=nullptr;reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"missing GUID blocked");f.type.guid=&f.guid;
 for(int offset:{-1,64,80}){f.guid.offset=offset;reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"out-of-bounds GUID blocked");}f.guid.offset=16;
 f.guid.size=8;reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"wrong GUID size blocked");f.guid.size=16;
 f.guid.value.path=L"/Script/Other.Guid";reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"wrong reflected GUID type blocked");f.guid.value.path=L"/Script/CoreUObject.Guid";
 f.skip.dim=2;reject([&]{HelpyNpcGuards::TemporaryClass(&f.type);},"array skip field blocked");f.skip.dim=1;
 HelpyNpcGuards::Exclude(&f.actor);f.actor.bytes[0]=0;reject([&]{HelpyNpcGuards::VerifyExcluded(&f.actor);},"construction reverted skip flag");f.actor.bytes[0]=1;
 f.actor.bytes[16]=1;reject([&]{HelpyNpcGuards::VerifyExcluded(&f.actor);},"construction gave NPC a save identity");f.actor.bytes[16]=0;
 f.actor.flags=0;reject([&]{HelpyNpcGuards::VerifyExcluded(&f.actor);},"construction removed transient flag");
 auto l=loader(f);clockNow=100;
 check(l.FindHelpyNpc(&f.world,l.m_definitions[0])==&f.actor,"owned lease resolves without saved GUID scan");
 check(!l.FindHelpyNpc(&f.other,l.m_definitions[0]),"wrong world blocked");
 check(!l.FindHelpyNpc(&f.world,l.m_definitions[1]),"source permanent NPC is not adopted");
 ++f.actor.version;check(!l.FindHelpyNpc(&f.world,l.m_definitions[0]),"replaced actor slot cannot be adopted");--f.actor.version;
 clockNow=130;check(!l.FindHelpyNpc(&f.world,l.m_definitions[0]),"interaction at expiry rejected before cleanup tick");clockNow=100;
 l.m_helpyNpcs.push_back(l.m_helpyNpcs[0]);reject([&]{(void)l.FindHelpyNpc(&f.world,l.m_definitions[0]);},"duplicate lease blocked");l.m_helpyNpcs.pop_back();
 check(l.DismissHelpyNpcs(&f.other)==0&&f.actor.requests==0,"dismiss only matching world");
 check(l.DismissHelpyNpcs(&f.world)==1&&f.actor.requests==1,"dismiss requests native destruction");
 check(!l.m_definitions[0].Enabled&&l.m_definitions[1].Enabled,"source definition remains enabled");
 check(l.m_merchantBindings.size()==1&&l.m_merchantBindings[0].DefinitionKey=="M:source","only temporary merchant acknowledgement removed");
 check(l.m_applied.size()==1&&l.m_applied.contains("M:source"),"source setup state retained");
 check(l.m_npcNames.size()==1&&l.m_npcNames.contains(L"/World.Source"),"only owned actor/component names removed");
 check(l.m_spawnedVendors.size()==1&&l.m_spawnedVendors[0].Key=="M:source","source actor remains tracked");
 check(l.pendingCleanup.contains(&f.actor)&&!l.pendingCleanup.contains(&f.unrelated),"cleanup never owns unrelated actors");
 check(l.m_helpyNpcs.size()==1,"destroy requested is not destruction acknowledged");
 f.actor.destroyFails=true;l.PumpHelpyNpcs();check(l.m_helpyNpcs.size()==1&&l.errors==1,"native failure retains ownership for retry");
 f.actor.destroyFails=false;f.actor.usable=false;l.PumpHelpyNpcs();check(l.m_helpyNpcs.empty()&&l.pendingCleanup.contains(&f.actor),"confirmed unusable retires lease but normal cleanup owns root release");
 f.actor.usable=true;f.actor.requests=0;l=loader(f);++f.actor.version;l.PumpHelpyNpcs();check(l.m_helpyNpcs.empty()&&l.m_npcNames.size()==1&&l.m_applied.size()==1,"GC before pump still removes owned bookkeeping without actor access");--f.actor.version;
 l=loader(f);++f.world.version;l.PumpHelpyNpcs();check(f.actor.requests==1,"expired world handle retires owned NPC");--f.world.version;
 l=loader(f);l.m_helpyNpcs.push_back({PS::WeakObjectHandle(&f.unrelated),PS::WeakObjectHandle(&f.world),"M:other",PS::HelpyNodes::Lease::Start(100,30)});
 f.actor.throwUsable=true;check(l.DismissHelpyNpcs()==2,"one unknown native error cannot skip remaining NPCs");check(f.unrelated.requests==1,"later owned NPC still receives cleanup");f.actor.throwUsable=false;
 std::cout<<"PASS: "<<tested<<" actual NPC guard / lease-method assertions with reflection, object identity and native destruction stand-ins.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<tested<<": "<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='helpy-npc-tests-') as temp:
    td=Path(temp);(td/'Mock.h').write_text(mock)
    for name in ['SDK/Helper/ActorHelper.h','SDK/Helper/PropertyHelper.h','Unreal/UFunction.hpp','Unreal/AActor.hpp','Unreal/CoreUObject/UObject/UnrealType.hpp']:
        f=td/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('#pragma once\n#include "Mock.h"\n')
    cpp=td/'npc.cpp';cpp.write_text(program)
    flags=['-std=c++23','-Wall','-Wextra','-Werror','-pedantic','-I'+str(td),'-I'+str(root/'raw/include')]
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
    subprocess.run([args.compiler,*flags,str(cpp),'-o',str(td/'test')],check=True)
    subprocess.run([str(td/'test')],check=True)
