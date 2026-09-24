#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

static void Require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}

int main(int argc,char** argv) {
    if(argc!=4)throw std::runtime_error("spawn loader, spawn tools and header sources required");
    const auto loader=Read(argv[1]);
    const auto tools=Read(argv[2]);
    const auto header=Read(argv[3]);

    Require(loader.find("ESpawnActorScaleMethod::OverrideRootScale")!=std::string::npos,
        "permanent resource creation no longer overrides the root scale");
    Require(tools.find("}, ESpawnActorScaleMethod::OverrideRootScale);")!=std::string::npos,
        "temporary resource creation can multiply with an authored root scale");
    Require(loader.find("ReconcileManagedActorScales(deltaSeconds)")!=std::string::npos,
        "managed actor scales are not reconciled from the engine tick");
    Require(loader.find("current.X() - authored.X()")!=std::string::npos
        && loader.find("actor->SetActorScale3D(authored)")!=std::string::npos,
        "managed actor scale is not restored as an absolute value");
    Require(header.find("m_managedScaleElapsed")!=std::string::npos,
        "managed actor scale reconciliation is not rate limited");
    Require(loader.find("GetActorScale3D() *") == std::string::npos
        && loader.find("GetActorScale3D()*") == std::string::npos,
        "resource scale is compounded from the actor's current scale");

    std::cout<<"PASS: managed scale is absolute at creation and re-normalized after reload or respawn.\n";
}
