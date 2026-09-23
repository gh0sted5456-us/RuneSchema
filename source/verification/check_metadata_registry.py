"""Compile the shipped metadata registry with a test-only weak-object lifetime adapter.
The mutex/map/declaration/incomplete-state implementation is production C++.
The UObject and weak identity adapter are stand-ins, not engine validation.
"""
from pathlib import Path
import argparse, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--sanitize',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1]
stub='''#pragma once
namespace RC::Unreal {class UObject {public: int serial=1;bool alive=true;};}
namespace PS {struct WeakObjectHandle {
 RC::Unreal::UObject* object=nullptr;int serial=0;
 void Assign(RC::Unreal::UObject* value){object=value;serial=value?value->serial:0;}
 RC::Unreal::UObject* Get()const noexcept{return object&&object->alive&&object->serial==serial?object:nullptr;}
};}
'''
test=r'''
#include "Loader/AssetMetadataRegistry.h"
#include "SDK/WeakObjectHandle.h"
#include <iostream>
#include <stdexcept>
namespace M=PS::AssetMetadata;
int count=0;void check(bool v,const char* text){++count;if(!v)throw std::runtime_error(text);}
int main(){try {
 M::Clear();RC::Unreal::UObject obj;M::Declaration d;d.safeToClone=true;d.runeSchema=true;d.modded=true;
 check(!M::IsManaged(&obj)&&M::Lookup(&obj).Empty(),"unknown is not fabricated");
 M::Record(&obj,d,"Example",false);
 check(M::IsManaged(&obj)&&M::Lookup(&obj).safeToClone.value_or(false),"records author metadata");
 check(!M::HasInstalledDefinition(&obj),"temporary source not called installed");
 M::Record(&obj,{},"Installed",true);check(M::HasInstalledDefinition(&obj),"installed state recorded");
 M::Declaration deny;deny.safeToClone=false;deny.runeSchema=false;M::Record(&obj,deny,"Override",false);
 check(M::Lookup(&obj).safeToClone.has_value()&&!M::Lookup(&obj).safeToClone.value(),"false permission overrides true");
 check(!M::Lookup(&obj).runeSchema.value_or(true),"false attribution retained");
 M::MarkIncomplete(&obj);check(M::IsIncomplete(&obj)&&!M::HasInstalledDefinition(&obj),"incomplete publication quarantined");
 M::Record(&obj,d,"Later edit",true);check(M::IsIncomplete(&obj)&&!M::HasInstalledDefinition(&obj),"ordinary patch cannot clear quarantine");
 ++obj.serial;check(!M::IsManaged(&obj)&&M::Lookup(&obj).Empty()&&!M::IsIncomplete(&obj),"reused pointer cannot inherit old permissions");
 M::Record(&obj,d,"New lifetime",true);check(M::HasInstalledDefinition(&obj),"new identity accepts new record");
 obj.alive=false;check(!M::IsManaged(&obj),"destroyed object dropped");obj.alive=true;
 M::Record(&obj,d,"Replacement",true);M::Forget(&obj);check(!M::IsManaged(&obj),"rollback forgets metadata");
 M::Record(nullptr,d,"Ignored",true);M::MarkIncomplete(nullptr);check(!M::IsManaged(nullptr),"null calls are safe");
 M::Record(&obj,d,"Final",true);M::Clear();check(!M::IsManaged(&obj),"shutdown clears session records");
 std::cout<<"PASS: "<<count<<" shipped metadata-registry assertions; UObject/weak identity mocked.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='helpy-metadata-') as tmp:
 d=Path(tmp);(d/'SDK').mkdir();(d/'SDK/WeakObjectHandle.h').write_text(stub);(d/'test.cpp').write_text(test)
 cmd=[args.compiler,'-std=c++23','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I'+str(d),'-I'+str(root/'raw/include')]
 if args.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
 cmd += [str(root/'raw/src/Loader/AssetAuthoringMetadata.cpp'),str(d/'test.cpp'),'-o',str(d/'test')]
 subprocess.run(cmd,check=True);subprocess.run([str(d/'test')],check=True)
