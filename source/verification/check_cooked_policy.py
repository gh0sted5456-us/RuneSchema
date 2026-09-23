"""Compile the shipped loaded-package policy with bounded UObject stand-ins.
This does NOT link the native UE4SS types or exercise Unreal memory/GC.
"""
from pathlib import Path
import subprocess,tempfile,argparse
p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--sanitize',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1]
text=(root/'raw/include/SDK/Helper/CookedAssetLookup.h').read_text()
code=text[text.index('template<class Pointer> inline auto* RawObjectPointer'):text.index('inline nlohmann::json VisualRoster')]
prefix=r'''
#include <set>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cstdint>
namespace RC {inline std::string to_string(const std::string& s){return s;}
namespace Unreal {
enum EObjectFlags : std::uint64_t {RF_Transient=1,RF_ClassDefaultObject=2,RF_ArchetypeObject=4,RF_DefaultSubObject=8,
 RF_NeedInitialization=16,RF_NeedLoad=32,RF_NeedPostLoad=64,RF_NeedPostLoadSubobjects=128,RF_BeginDestroyed=256,
 RF_FinishDestroyed=512,RF_WasLoaded=1024,RF_Public=2048};
struct Type {std::string name;const std::string& GetName()const{return name;}};
struct UObject {Type* type=nullptr;UObject* outer=nullptr;std::string path,name;std::uint64_t flags=0;
 Type* GetClassPrivate(){return type;}bool HasAnyFlags(EObjectFlags f){return (flags&f)!=0;}
 UObject* GetOuterPrivate(){return outer;}const std::string& GetPathName(){return path;}const std::string& GetName(){return name;}
};
}}
namespace PS {
namespace AssetProvenance {
inline bool known=false,confirmed=false;inline std::string kind;
struct Provenance {bool is_object()const{return known;}std::string value(const char*,const std::string& fallback) const{return known?kind:fallback;}
 bool value(const char*,bool fallback) const{return known?confirmed:fallback;}};
inline Provenance Lookup(RC::Unreal::UObject*){return {};}
}
namespace CookedAssets {
inline bool ready=false;inline std::set<std::string> registered;
inline bool Available(){return ready;}inline bool ContainsReference(const std::string& path){return registered.contains(path);}
'''
suffix=r'''
}}
using namespace RC::Unreal;namespace C=PS::CookedAssets;namespace P=PS::AssetProvenance;
int checks=0;void check(bool y,const char* why){++checks;if(!y)throw std::runtime_error(why);}
struct ObjectHandle {UObject* value;UObject* Get(){return value;}};
int main(){try {
 Type packageType{"Package"},itemType{"ItemData"},wrongType{"Actor"};
 UObject package{&packageType,nullptr,"/Game/Items/Apple","Apple",0};
 UObject object{&itemType,&package,"/Game/Items/Apple.Apple","Apple",RF_Public|RF_WasLoaded};
 check(C::RawObjectPointer(&object)==&object,"raw pointer accessor");check(C::RawObjectPointer(ObjectHandle{&object})==&object,"TObjectPtr accessor");
 check(C::VerifiedLoadedObject(&object),"loaded public package asset fallback");
 for(auto flag:{RF_Transient,RF_ClassDefaultObject,RF_ArchetypeObject,RF_DefaultSubObject,RF_NeedInitialization,RF_NeedLoad,RF_NeedPostLoad,RF_NeedPostLoadSubobjects,RF_BeginDestroyed,RF_FinishDestroyed}){
  object.flags|=flag;check(!C::VerifiedLoadedObject(&object),"unsafe object rejected");object.flags&=~std::uint64_t(flag);
 }
 object.flags=RF_Public;check(!C::VerifiedLoadedObject(&object),"/Game runtime object not cooked");object.flags=RF_WasLoaded;check(!C::VerifiedLoadedObject(&object),"non-public object rejected");object.flags=RF_Public|RF_WasLoaded;
 check(!C::VerifiedLoadedObject(nullptr),"null object rejected");object.type=nullptr;check(!C::VerifiedLoadedObject(&object),"null type rejected");object.type=&itemType;
 object.outer=nullptr;check(!C::VerifiedLoadedObject(&object),"no package rejected");object.outer=&package;
 package.type=&wrongType;check(!C::VerifiedLoadedObject(&object),"actor-owned object rejected");package.type=&packageType;
 package.flags=RF_Transient;check(!C::VerifiedLoadedObject(&object),"transient package rejected");package.flags=0;
 for(const auto path:{"/Temp/Apple","/Engine/Transient","/Script/Game","/Game/A.B","/Game/A:B"}){package.path=path;object.path=package.path+".Apple";check(!C::VerifiedLoadedObject(&object),"temporary/script/subobject package rejected");}
 package.path="/Game/Items/Apple";object.path="/Game/Items/Other.Other";check(!C::VerifiedLoadedObject(&object),"actual identity must match package");object.path="/Game/Items/Apple.Apple";
 P::known=true;P::confirmed=true;P::kind="RuneSchemaAssetClone";check(!C::VerifiedLoadedObject(&object),"known runtime clone denied even with copied flags");P::known=false;P::confirmed=false;
 C::ready=true;check(!C::VerifiedLoadedObject(&object),"available registry miss not bypassed by flags");C::registered.insert(object.path);check(C::VerifiedLoadedObject(&object),"registry match accepted");
 P::known=true;P::confirmed=true;check(!C::VerifiedLoadedObject(&object),"provenance wins over registry match");P::known=false;
 C::ready=false;package.path="/Fellhollow/Items/Apple";object.path=package.path+".Apple";check(C::VerifiedLoadedObject(&object),"installed plugin package allowed");
 std::cout<<"PASS: "<<checks<<" shipped cooked-package policy assertions using UObject stand-ins.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory() as td:
 source=Path(td)/'policy.cpp';source.write_text(prefix+code+suffix)
 output=Path(td)/'policy'
 command=[args.compiler,'-std=c++23','-Wall','-Wextra','-Werror','-pedantic',str(source),'-o',str(output)]
 if args.sanitize:command += ['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 subprocess.run(command,check=True);subprocess.run([str(output)],check=True)
