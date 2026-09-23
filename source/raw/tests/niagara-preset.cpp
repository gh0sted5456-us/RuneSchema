#include "Generator/NiagaraPreset.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace PS::NiagaraPreset;
template<class F> void rejects(F f) {bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main(int argc,char** argv) {
    for(size_t i=0;i<4;++i) {
        const auto preset=Builtin(i);
        assert(Validate(Parse("// shared test\n"+preset.dump()))==preset);
        if(i) {unsigned enabled=0;for(const auto& value:preset["Emitters"])if(value==true)++enabled;assert(enabled==1);}
    }
    auto p=Builtin(0);p["Target"]="PlayerMesh";p["Socket"]="hand_r";assert(Validate(p)==p);
    p=Builtin(0);p["LocationOffset"]={{"X",12.5},{"Y",-4.0},{"Z",80.0}};
    p["RotationOffset"]={{"Pitch",0.0},{"Yaw",90.0},{"Roll",15.0}};assert(Validate(p)==p);
    p=Builtin(0);p["LocationOffset"]["Z"]="high";rejects([&]{Validate(p);});
    p=Builtin(0);p["RotationOffset"]={{"Pitch",0.0},{"Yaw",0.0}};rejects([&]{Validate(p);});
    p["Target"]="PlayerRoot";rejects([&]{Validate(p);});
    p=Builtin(0);p["AutoStart"]=true;rejects([&]{Validate(p);});
    p=Builtin(0);p["System"]="/Game/../Asset.Asset";rejects([&]{Validate(p);});
    p=Builtin(0);p["System"]="/Script/Niagara.Component:Function";rejects([&]{Validate(p);});
    p=Builtin(0);p["Emitters"]["Smoke"]="false";rejects([&]{Validate(p);});
    p=Builtin(0);for(int i=0;i<17;++i)p["Emitters"][std::to_string(i)]=true;rejects([&]{Validate(p);});
    p=Builtin(0);p["Name"]=std::string("a\0b",3);rejects([&]{Validate(p);});
    p=Builtin(0);p["Version"]=1.0;rejects([&]{Validate(p);});
    rejects([]{Parse(std::string(16385,' '));});
    rejects([]{Parse(std::string(20,'[')+"0"+std::string(20,']'));});
    if(argc==2) {
        size_t checked=0;
        for(const auto& entry:std::filesystem::directory_iterator(argv[1])) {
            if(!entry.is_regular_file() || (entry.path().extension()!=".json" && entry.path().extension()!=".jsonc"))continue;
            if(entry.file_size()>16384)throw std::runtime_error("Preset file exceeds limit");
            std::ifstream file(entry.path());
            const std::string text{std::istreambuf_iterator<char>(file),{}};
            Validate(Parse(text));++checked;
        }
        if(!checked)throw std::runtime_error("No Niagara presets found");
        std::cout<<checked<<" Niagara preset files validated.\n";
    }
}
