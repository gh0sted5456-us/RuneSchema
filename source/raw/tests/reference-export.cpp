#include "Generator/ReferenceExport.h"
#include "Generator/LoaderCapabilities.h"
#include <cassert>
using namespace PS;
int main() {
    using nlohmann::json;
    for(const auto& loader:LoaderCapabilities) {
        auto report=ReferenceExport::Build(loader.Name,{{"Path","/Game/Mods/Test/Test.Test"}},
            {{"Name","Example"},{"Icon","/Game/UI/Icon.Icon"},{"Weight",0},{"Empty",""}},
            {{"Unknown",{{"type","string"}}},{"Name",{{"type","string"}}}});
        assert(report["Values"]["Unknown"].is_null());
        assert(report["Values"]["Weight"]==0 && report["Values"]["Empty"]=="");
        assert(report["Origin"].get<std::string>().starts_with("Unknown"));
        assert(report["ReferencedPaths"].size()==2);
        assert(!report["Truncated"].get<bool>());
        auto output=ReferenceExport::Jsonc(report);
        assert(json::parse(output,nullptr,true,true)==report);
        assert(output.find("Do not place")!=std::string::npos);
    }
    auto report=ReferenceExport::Build("buildings",{{"Authored",true}},{{"$Clone","/Game/A.A"}},json::object());
    assert(report["DeclaredCloneSource"]=="/Game/A.A");
    report=ReferenceExport::Build("assets",json::object(),{{"$Clone","/Game/A.A"}},json::object());
    assert(!report.contains("DeclaredCloneSource"));
    json fields=json::object();for(int i=0;i<300;++i)fields[std::to_string(i)]=i;
    report=ReferenceExport::Build("raw",json::object(),fields,json::object());
    assert(report["Truncated"]==true && report["Values"].size()==256);
}
