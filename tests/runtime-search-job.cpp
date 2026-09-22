#include "Runtime/SearchJob.h"
#include <cassert>
int main() {
    using nlohmann::json;
    using PS::RuntimeJobs::Validate;
    json job = {{"Name","CowDiscovery"},{"Enabled",true},{"Trigger","world-load"},{"Query","Cow"},{"MaxResults",100},{"DelaySeconds",3}};
    Validate(job);
    for (const auto* trigger : {"game-load", "world-load", "game-state-ready"}) { job["Trigger"]=trigger; Validate(job); }
    auto reject = [&](const char* key, json value) {
        auto bad = job; bad[key] = value;
        bool rejected = false;
        try { Validate(bad); } catch (const std::exception&) { rejected = true; }
        assert(rejected);
    };
    reject("Name", "../overwrite"); reject("Query", ""); reject("Trigger", "tick");
    reject("MaxResults", 251); reject("MaxResults", 0); reject("DelaySeconds", -1); reject("DelaySeconds", 31);
    reject("Enabled", "true"); reject("ClassContains", 5); reject("Output", "C:/arbitrary.json");
    auto comments = json::parse("{/* comment */\"Name\":\"Test\",\"Trigger\":\"game-load\",\"Query\":\"Vendor\"}", nullptr, true, true);
    Validate(comments);
}
