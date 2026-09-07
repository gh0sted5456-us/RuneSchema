#include "Utility/Config.h"
#include "glaze/glaze.hpp"
#include <cassert>
#include <iostream>
int main(){
    PS::PSConfigSettings settings;
    auto error=glz::read<glz::opts{.error_on_missing_keys=false}>(settings,R"({"enableAutoReload":false,"loaders":{"assets":false,"players":true}})");
    assert(!error && settings.loaders.equipment && !settings.loaders.assets && !settings.enableAutoReload);
    error=glz::read<glz::opts{.error_on_missing_keys=false}>(settings,R"({"loaders":{"equipment":false}})");
    assert(!error && !settings.loaders.equipment);
    std::cout<<"PASS: older settings retain values and default equipment on; explicit equipment disable is accepted.\n";
}
