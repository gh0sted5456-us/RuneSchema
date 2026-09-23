"""Compile the shipped Matches predicate with the baseline's real WeakObjectHandle.
Unreal object slots are stand-ins. This is NOT an actual GC/ABI/in-game test.
"""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1]
s=(r/'raw/src/Loader/AssetProvenance.cpp').read_text()
start=s.index('bool Matches(');body=s.index('{',start);end=body+1;depth=1
while depth:
 if s[end]=='{':depth+=1
 elif s[end]=='}':depth-=1
 end+=1
function=s[start:end]
with tempfile.TemporaryDirectory(prefix='f2-provenance-') as tmp:
 t=Path(tmp);(t/'Unreal').mkdir()
 (t/'Unreal/UObject.hpp').write_text('''#pragma once
#include <cstdint>
namespace RC::Unreal {struct UObject {int32_t index=0;int32_t GetInternalIndex()const{return index;}};}
''')
 (t/'Unreal/UObjectArray.hpp').write_text('''#pragma once
#include "Unreal/UObject.hpp"
#include <array>
namespace RC::Unreal {
struct Slot {UObject* object=nullptr;int32_t serial=0;bool valid=true;
UObject* GetUObject()const{return object;}int32_t& GetSerialNumber(){return serial;}
bool IsValid(bool)const{return valid&&object;}};
struct FUObjectArray {inline static std::array<Slot,4> slots{};
static Slot* IndexToObject(int32_t i){return i>=0&&i<4?&slots[i]:nullptr;}
static int32_t GetNumElements(){return 4;}};}
''')
 code='''#include "WeakObjectHandle.h"
#include <iostream>
#include <stdexcept>
struct Entry {RC::Unreal::UObject* token=nullptr;PS::WeakObjectHandle handle;};
'''+function+'''
int checks=0;void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
int main(){using namespace RC::Unreal;try{
 UObject object{0},other{1},replacement{0};auto& slot=FUObjectArray::slots[0];slot.object=&object;
 FUObjectArray::slots[1]={&other,17,true};
 const auto oldSerial=slot.serial;check(oldSerial==0,"original zero-serial starting condition");
 Entry entry;entry.token=&object;entry.handle.Assign(&object);
 check(slot.serial>17,"shared weak handle allocates serial before recording identity");
 check(oldSerial!=slot.serial,"negative control: old zero-serial record becomes stale");
 check(Matches(entry,&object),"new tracking survives serial allocation");
 PS::WeakObjectHandle later(&object);check(Matches(entry,&object)&&later.Get()==entry.handle.Get(),"later weak reference preserves existing creation record");
 check(!Matches(entry,&other)&&!Matches(entry,nullptr),"unrelated or null object cannot reuse provenance");
 slot.valid=false;check(!Matches(entry,&object),"invalid native slot denied");slot.valid=true;
 slot.object=nullptr;check(!Matches(entry,&object),"expired object rejected");
 slot.object=&replacement;slot.serial=0;PS::WeakObjectHandle newHandle(&replacement);
 check(!Matches(entry,&replacement)&&!Matches(entry,&object),"slot reuse cannot inherit old creator");
 entry.token=&replacement;entry.handle=newHandle;check(Matches(entry,&replacement),"new lifetime tracked independently");
 const auto serial=slot.serial;slot.serial=serial+1;check(!Matches(entry,&replacement),"same address with new serial loses prior provenance");
 entry.handle.Reset();check(!Matches(entry,&replacement),"cleared handle has no origin proof");
 std::cout<<"PASS: "<<checks<<" provenance identity checks, including original zero-serial negative control. Native slots mocked.\\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\\n';return 1;}}
'''
 (t/'test.cpp').write_text(code)
 flags=['-std=c++23','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I'+str(t),'-I'+str(r/'verification/fixtures')]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
 subprocess.run([a.compiler,*flags,str(t/'test.cpp'),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True)
