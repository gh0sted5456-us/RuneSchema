#include "Core/AdditionalLoggingConfiguration.h"
#include <chrono>
#include <limits>
#include <stdexcept>
#define Need(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(){
    using namespace PS;using namespace AdditionalLoggingInfo;using Json=nlohmann::json;
    RateWindow rates;Need(!rates.Sample(1,{{1,100,100}}));const auto value=rates.Sample(3,{{1,2000100,1000100}});
    Need(value&&value->DownloadMbps==8&&value->UploadMbps==4&&value->Interfaces==1);
    Need(!rates.Sample(4,{{2,99999999,9999999}})); // New interface has no comparable baseline.
    Need(!rates.Sample(5,{{2,1,1}})); // Counter reset/wrap never becomes huge throughput.
    Need(!rates.Sample(5,{{2,2,2}}));Need(!rates.Sample(6,{{2,3,3},{2,3,3}}));
    rates.Reset();Need(!rates.Sample(7,{{2,1000,1000}}));Need(!rates.Sample(std::numeric_limits<double>::quiet_NaN(),{}));
    LatencyMean latency;Need(!latency.Value());Need(!latency.Add(0)&&!latency.Add(-1)&&!latency.Add(60001)&&!latency.Add(std::numeric_limits<double>::quiet_NaN()));
    Need(latency.Add(40)&&latency.Add(60)&&latency.Value()==50);
    const auto root=std::filesystem::temp_directory_path()/("additional-logging-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file=root/"settings.ini";auto document=SettingsDocument(PSConfigSettings{});
    document["persistence"]["allowCorruptCharacterEntry"]=true;document["quickLaunch"]={{"mode","quickLaunch"},{"enabled",true},{"characterName","BREWER"},{"worldName","World"},{"softLoading",true}};
    document["custom"]={{"retained",true}};ConfigFiles::Write(file,EncodeIniDocument(document));
    const auto initial=AdditionalLoggingConfiguration::Read(root);Need(initial.at("Ok")==true&&AdditionalLoggingConfiguration::Bits(initial.at("Document"))==0);
    auto choices=initial.at("Document");choices["enabled"]=true;choices["networkRates"]=true;choices["averageLatency"]=true;
    const auto before=ConfigFiles::Read(file);
    const auto save=[&](const Json& options,const Json& expected){return AdditionalLoggingConfiguration::Save(root,{{"ExpectedContents",expected},{"Document",options}});};
    const auto saved=save(choices,before);Need(saved.at("Ok")==true);Need(ConfigFiles::Read(saved.at("Backup").get<std::string>())==before);
    const auto current=DecodeIniDocument(ConfigFiles::Read(file));Need(current.at("quickLaunch")==document.at("quickLaunch")&&current.at("persistence")==document.at("persistence")&&current.at("custom")==document.at("custom"));
    Need(AdditionalLoggingConfiguration::Bits(saved.at("Document"))==(Enabled|NetworkRates|AverageLatency));
    Need(!saved.at("Document").at("filePaths").get<bool>()&&!saved.at("Document").at("region").get<bool>()&&!saved.at("Document").at("savePaths").get<bool>());
    const auto committed=ConfigFiles::Read(file);Need(save(choices,before).at("Ok")==false&&ConfigFiles::Read(file)==committed);
    choices["enabled"]="true";bool rejected=false;try{(void)save(choices,committed);}catch(...){rejected=true;}Need(rejected&&ConfigFiles::Read(file)==committed);
    choices=saved.at("Document");choices["enabled"]=false;Need(save(choices,committed).at("Ok")==true);
    // Only this exact temporary test fixture is removed. No game data is accessed.
    std::filesystem::remove_all(root);
}
