// Optional integration test against the project's actual nlohmann/json.hpp.
// g++ -std=c++20 -I raw/include -I <json-include-dir> test_mailbox.cpp -o mailbox-tests
#include "Generator/ToolRequest.h"
#include <cstdlib>
#include <iostream>
using PS::SpawnToolRequests;
using nlohmann::json;
static int checks=0;
void Check(bool value,const char* why){++checks;if(!value){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    SpawnToolRequests::Available=true;SpawnToolRequests::Clear();
    Check(SpawnToolRequests::TrySubmit({{"Action","IndexQuickCatalog"},{"_RequestId",1}}),"index start accepted");
    Check(SpawnToolRequests::Take().has_value(),"index start dequeued");
    SpawnToolRequests::CatalogProgress({{"Definitions",json::array({{{"Key","@loaded-ai:/Example.AI_C"}}})},{"_CatalogIndexing",true}});
    SpawnToolRequests::Publish({{"Status","scan started"}});
    Check(!SpawnToolRequests::InFlight&&!SpawnToolRequests::Waiting,"index start immediately releases mutation slot");
    json receipt;Check(SpawnToolRequests::TakeCompleted(1,receipt),"one-shot start acknowledgement");
    Check(SpawnToolRequests::TrySubmit({{"Action","Spawn"},{"_RequestId",2}}),"spawn admitted during catalog scan");
    Check(SpawnToolRequests::Take().has_value(),"spawn dequeued");
    const auto active=SpawnToolRequests::ActiveRequest;
    SpawnToolRequests::CatalogProgress({{"Definitions",json::array({{{"Key","@loaded-resource:/Example.Stone_C"}}})},{"_CatalogIndexing",false},
        {"_RequestId",99},{"Status","must not overwrite action"},{"SpawnResult",{{"State","bogus"}}}});
    Check(SpawnToolRequests::InFlight&&SpawnToolRequests::Waiting&&SpawnToolRequests::ActiveRequest==active,"scan completion cannot release active spawn");
    Check(!SpawnToolRequests::TakeCompleted(2,receipt),"scan cannot fabricate spawn receipt");
    Check(!SpawnToolRequests::TrySubmit({{"Action","Spawn"},{"_RequestId",3}}),"overlapping spawn refused");
    SpawnToolRequests::Publish({{"Status","spawned one"},{"SpawnResult",{{"State","Completed"},{"Created",1}}}});
    Check(SpawnToolRequests::TakeCompleted(2,receipt)&&receipt["SpawnResult"]["Created"]==1,"matching completion contains actual created count");
    Check(!SpawnToolRequests::TakeCompleted(2,receipt),"receipt is one shot");
    Check(SpawnToolRequests::TrySubmit({{"Action","InspectClone"},{"_RequestId",22}}),"details request accepted");
    Check(SpawnToolRequests::Take().has_value(),"details request dequeued");
    SpawnToolRequests::Publish({{"Status","details loaded"},{"ItemDetails",{{"Fields",json::array({{{"Name","PowerLevel"},{"Value",7}}})},{"Relations",{{"Recipes",json::array({{{"Name","Platelegs recipe"}}})}}}}}});
    Check(SpawnToolRequests::TakeCompleted(22,receipt)&&receipt["ItemDetails"]["Fields"].size()==1
        &&receipt["ItemDetails"]["Relations"]["Recipes"].size()==1,"details receipt retains reflected fields and recipe relations");
    Check(SpawnToolRequests::TrySubmit({{"Action","Catalog"}}),"settings request accepted");
    Check(SpawnToolRequests::Take().has_value(),"settings request dequeued");
    const json saved=json::array({{{"Key","saved-settings-choice"}}});
    SpawnToolRequests::Publish({{"Status","saved catalog"},{"Definitions",saved}});
    SpawnToolRequests::CatalogProgress({{"Definitions",json::array({{{"Key","live-f2-choice"}}})},{"_CatalogIndexing",true}});
    Check(SpawnToolRequests::Read()["Definitions"]==saved,"background scan leaves saved settings catalog untouched");
    uint64_t revision=0;json view;
    Check(SpawnToolRequests::ReadQuickIfChanged(revision,view)&&view["Definitions"][0]["Key"]=="live-f2-choice","only F2 reads live index stream");
    Check(!SpawnToolRequests::ReadQuickIfChanged(revision,view),"unchanged revision not copied");
    SpawnToolRequests::CancelRequested=true;
    Check(SpawnToolRequests::TrySubmit({{"Action","Spawn"},{"_RequestId",4}}),"next spawn can queue");
    Check(SpawnToolRequests::CancelRequested,"queue does not undo scan cancellation");
    const auto generation=SpawnToolRequests::Generation.load();SpawnToolRequests::Clear();
    Check(SpawnToolRequests::Generation.load()!=generation,"world reset invalidates index generation");
    Check(SpawnToolRequests::QuickCatalogData.empty()&&!SpawnToolRequests::Waiting&&!SpawnToolRequests::InFlight,"world reset clears catalogs and pending mutation");
    std::cout<<checks<<" real JSON mailbox assertions passed.\n";
}
