#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    if(argc!=4)throw std::runtime_error("Quest service and stage progress sources required");
    const auto read=[](const char* path) {
        std::ifstream file(path);if(!file)throw std::runtime_error("Cannot read quest marker source");
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto source=read(argv[1]),runtime=read(argv[2]),native=read(argv[3]);
    const auto require=[](bool ok){if(!ok)throw std::runtime_error("Quest marker readiness regression");};
    require(source.find("failedLocations.insert(key)")==source.npos);
    const auto create=source.find("owned->Create(");
    const auto failure=source.find("failedLocations.insert(markerKey)");
    require(create!=source.npos && failure>create && failure<source.find("locations.emplace(markerKey",create));
    const auto markerGate=source.find("if(!definition.Marker && definition.Stages.empty())continue;");
    require(markerGate!=source.npos && markerGate<source.find("const QuestNative::Adapter native(controller,Asset(key));",markerGate));
    require(source.find("!failedLocations.contains(markerKey)")!=source.npos);
    // Authority keeps the native initialization guard. A connected client may
    // only bypass it while reading a fingerprint-validated replicated receipt
    // for local AOR presentation.
    require(source.find("StageProgress(native,definition,run,canMutate)")!=source.npos);
    require(runtime.find("bool requireInitialized=true")!=runtime.npos);
    require(runtime.find("OpenNative(api,StageGraph(quest),run,requireInitialized)")!=runtime.npos);
    require(native.find("if(requireInitialized && !api.IsInitialized())throw std::runtime_error(\"Quest stage save record is not initialized\")")!=native.npos);
}
