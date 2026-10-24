#include "Runtime/SearchJob.h"
#include <cassert>
#undef assert
#define assert(value) ((value)?static_cast<void>(0):throw std::runtime_error(#value))
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
    PS::RuntimeJobs::SearchCursor cursor(900000);
    for(size_t index=0;index<500000;++index)assert(cursor.Next(900000)==index);
    assert(!cursor.Next(900000));
    PS::RuntimeJobs::SearchCursor shrinking(10);assert(shrinking.Next(10)==0);assert(!shrinking.Next(0));
    PS::RuntimeJobs::SearchCursor growing(2);assert(growing.Next(20)==0);assert(growing.Next(20)==1);assert(!growing.Next(20));
}
