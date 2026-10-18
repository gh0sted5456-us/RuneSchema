#include "Core/HelpyIconPack.h"
#include <iostream>
// Explicit offline verification utility; never invoked by the test runner or
// shipped with the mod. Reads a completed native cache, writes new workspace
// outputs, and exercises the SAME pack implementation used by Helpy.
int main(int argc,char** argv)try{
    using namespace PS;namespace fs=std::filesystem;using Json=nlohmann::json;
    if(argc!=6)throw std::runtime_error("Arguments: RuneSchema-root cache-directory build-report output-pack isolated-import-directory");
    const fs::path root=argv[1],cache=argv[2],report=argv[3],output=argv[4],imported=argv[5];
    if(fs::exists(output)||fs::exists(imported))throw std::runtime_error("Verification outputs must not already exist");
    const auto read=[](const fs::path& path,size_t limit){return HelpyIconPack::Json(HelpyIconPack::Read(path,limit));};
    const auto document=read(cache/"sources.json",1024*1024);HelpySourceCache::AcceptRemote(document);const auto sources=HelpySourceCache::Current;
    const auto coverage=read(report,16*1024*1024);if(!IconCacheCoverage::Complete(coverage))throw std::runtime_error("Cache build is not complete");
    HelpyIconPack::Progress progress;std::cout<<"Exporting completed cache..."<<std::endl;
    const auto exported=HelpyIconPack::Export(output,root,cache,sources,progress,"",coverage);
    std::cout<<exported.Message<<std::endl;
    std::ifstream file(output,std::ios::binary);uint64_t start=0;const auto manifest=HelpyIconPack::Header(file,start,fs::file_size(output));
    if(!manifest.Coverage.value("Complete",false)||exported.Exported!=coverage.at("Total").get<size_t>())throw std::runtime_error("Full export coverage mismatch");
    for(const auto& entry:manifest.Icons)(void)HelpyIconPack::Blob(file,entry,start);
    file.close();std::cout<<"Checksums validated; importing into isolated workspace cache..."<<std::endl;
    const auto restored=HelpyIconPack::Import(output,root,imported,sources,512ull*1024*1024,progress);
    if(restored.Imported!=exported.Exported||restored.Skipped)throw std::runtime_error("Isolated import coverage mismatch");
    const auto unchanged=HelpyIconPack::Import(output,root,imported,sources,512ull*1024*1024,progress);
    if(unchanged.Imported||unchanged.Unchanged!=exported.Exported||unchanged.Skipped)throw std::runtime_error("Unchanged round-trip did not retain every icon");
    Json receipt={{"Exported",exported.Exported},{"Imported",restored.Imported},{"UnchangedSecondImport",unchanged.Unchanged},{"Skipped",unchanged.Skipped},{"Bytes",fs::file_size(output)},{"Coverage",manifest.Coverage},{"SourceKeys",sources.SourceKeys}};
    const auto text=receipt.dump(2);HelpyIconPack::Publish(output.wstring()+L".verification.json",{text.begin(),text.end()},false);
    std::cout<<"PASS: "<<exported.Exported<<" icons exported, checksummed, imported and retained on second import."<<std::endl;return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 1;}
