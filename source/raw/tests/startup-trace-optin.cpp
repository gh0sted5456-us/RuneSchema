
#include <filesystem>
#include <cassert>
#include <iostream>
#include "Utility/Config.h"
#include "Utility/StartupTrace.h"
int main(){auto path=std::filesystem::temp_directory_path()/"RuneSchema-optin-unit-test";
std::filesystem::remove_all(path);PS::StartupTrace::Begin(path);PS::StartupTrace::Mark("disabled");assert(!std::filesystem::exists(path));
PS::PSConfig::Get()->enabled=true;PS::StartupTrace::Mark("enabled");assert(std::filesystem::file_size(path/"startup-current.log")>0);
auto bytes=std::filesystem::file_size(path/"startup-current.log");PS::PSConfig::Get()->enabled=false;PS::StartupTrace::Mark("disabled again");assert(std::filesystem::file_size(path/"startup-current.log")==bytes);
std::cout<<"PASS actual startup trace: no disk activity when disabled; logging opt-in and disabling respected\n";}
