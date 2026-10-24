#pragma once
#include <nlohmann/json.hpp>
namespace PS::CatalogWindowProgress {
// Keep asynchronous cache waits distinct from a non-advancing native scan.
// The transport must not throw these fields away when wrapping a page.
inline void Forward(nlohmann::json& window,const nlohmann::json& page){
    for(const auto* key:{"PopulationStage","PopulationDone","PopulationTotal","CheckingChanges","CursorRecovered","CachePreview"})
        if(page.contains(key))window[key]=page[key];
    window["CacheHit"]=window.value("CacheHit",false)||page.value("CacheHit",false);
}
}
