#include "Utility/StartupTrace.h"
#include <fstream>
#include <cassert>
#include <iostream>
std::string read(const std::filesystem::path& path){std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    const auto folder=std::filesystem::current_path()/"trace-fixture";
    PS::StartupTrace::Begin(folder);
    PS::StartupTrace::Mark("phase-one");
    assert(read(folder/"startup-current.log").find("phase-one")!=std::string::npos);
    PS::StartupTrace::Begin(folder);
    PS::StartupTrace::Mark("phase-two");
    assert(read(folder/"startup-previous.log").find("phase-one")!=std::string::npos);
    assert(read(folder/"startup-current.log").find("phase-one")==std::string::npos);
    assert(read(folder/"startup-current.log").find("phase-two")!=std::string::npos);
    PS::StartupTrace::Begin(folder/"startup-current.log"/"invalid-directory");
    std::cout<<"PASS: startup checkpoints flush immediately, rotate the previous run and tolerate filesystem failure.\n";
}
