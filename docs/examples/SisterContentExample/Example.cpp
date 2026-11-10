#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

class SisterContentExample : public RC::CppUserModBase
{
public:
    SisterContentExample()
    {
        ModName = STR("SisterContentExample");
        RC::Output::send(STR("[SisterContentExample] UE4SS started the example DLL.\n"));
    }
};

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new SisterContentExample();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
