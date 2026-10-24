#include "Core/UE4SSLuaPropertySize.h"
#include <cassert>
#include <fstream>
#include <iterator>

int main(int argc, char** argv) {
    using PS::UE4SSLuaPropertySize::RepairObjectPtr;
    for (int missing : {0, -1}) {
        assert(RepairObjectPtr(missing, sizeof(void*)) && missing == sizeof(void*));
        assert(!RepairObjectPtr(missing, sizeof(void*)) && missing == sizeof(void*));
    }
    for (int existing : {1, 4, 8, 16, -2}) {
        const int before = existing;
        assert(!RepairObjectPtr(existing, sizeof(void*)) && existing == before);
    }
    for (int unverified : {0, -1, 1, 16}) {
        int missing = -1;
        assert(!RepairObjectPtr(missing, unverified) && missing == -1);
    }
    assert(argc == 2);
    std::ifstream file(argv[1]);
    const std::string source{std::istreambuf_iterator<char>(file), {}};
    const auto init = source.find("auto on_unreal_init()");
    const auto repair = source.find("PS::UE4SSCompatibility::PrepareLuaPropertySizes();", init);
    assert(init != source.npos && repair != source.npos && repair < source.find("MainLoader.Initialize();", init));
}
