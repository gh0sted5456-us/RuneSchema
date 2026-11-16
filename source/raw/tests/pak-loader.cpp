#include "Runtime/PakLoader.h"
#include "../../ue4ss/pak-loader/PakLoaderApi.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace PS::PakLoader;

void Check(bool condition, const char* description)
{
    if (!condition) throw std::runtime_error(description);
    std::cout << "PASS " << description << '\n';
}

void Write(const fs::path& file, const char* contents)
{
    if (!(std::ofstream(file, std::ios::binary) << contents))
        throw std::runtime_error("Could not write fixture");
}

int wmain(int argc, wchar_t** argv)
{
    try {
        if (argc != 3) throw std::runtime_error("Pass the built DLL and a fresh absolute fixture directory");
        const fs::path root(argv[2]);
        if (!root.is_absolute() || !fs::create_directory(root)) throw std::runtime_error("Invalid fixture directory");
        const auto a = root / "Alpha", b = root / "Beta", bad = root / "Bad";
        for (const auto& mod : {a, b, bad}) {
            fs::create_directories(mod / "paks");
            fs::copy_file(argv[1], mod / "runeschema-pakloader.dll");
        }
        const auto dllA = LoadLibraryW((a / "runeschema-pakloader.dll").c_str());
        const auto dllB = LoadLibraryW((b / "runeschema-pakloader.dll").c_str());
        const auto dllBad = LoadLibraryW((bad / "runeschema-pakloader.dll").c_str());
        Check(dllA && dllB && dllBad && dllA != dllB && dllBad != dllA && dllBad != dllB,
            "same-named root DLL copies map independently");
        const auto queryA = reinterpret_cast<Query>(GetProcAddress(dllA, QueryExport));
        const auto queryB = reinterpret_cast<Query>(GetProcAddress(dllB, QueryExport));
        const auto queryBad = reinterpret_cast<Query>(GetProcAddress(dllBad, QueryExport));
        Check(queryA && queryB && queryBad, "exports present in each copy");
        Write(a / "config.ini", "[RuneSchemaPakLoader]\r\nModName=Alpha\r\n");
        Write(a / "paks" / "A.pak", "fixture");
        Check(queryA() == Result::Ready, "single flat PAK");
        Check(queryA() == Result::Ready, "repeat query is stable");
        Check(queryB() == Result::InvalidConfig, "missing config is rejected");
        Write(b / "config.ini", "[RuneSchemaPakLoader]\r\nModName=Alpha\r\n");
        Check(queryB() == Result::InvalidConfig, "config cannot select a different mod");
        Write(b / "config.ini", "[RuneSchemaPakLoader]\r\nModName=Beta\r\n");
        Check(queryB() == Result::NoPaks, "empty paks folder");
        Write(b / "paks" / "B.pak", "fixture");
        Write(b / "paks" / "B.utoc", "fixture");
        Check(queryB() == Result::InvalidContainers, "incomplete IoStore set");
        Write(b / "paks" / "B.ucas", "fixture");
        Check(queryA() == Result::Ready && queryB() == Result::Ready,
            "independent PAK and IoStore roots");
        fs::create_directory(bad / "paks" / "nested");
        Write(bad / "config.ini", "[RuneSchemaPakLoader]\nModName=Bad\n");
        Check(queryBad() == Result::InvalidContainers, "nested content is rejected");
        const std::vector<fs::path> existing{a / "paks" / "."};
        Check(AlreadyRegistered(a / "paks", existing) && !AlreadyRegistered(b / "paks", existing),
            "deduplicate the existing directory without dropping a different mod");
        FreeLibrary(dllBad);
        FreeLibrary(dllB);
        FreeLibrary(dllA);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
