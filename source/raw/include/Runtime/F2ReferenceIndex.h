#pragma once
#include "Generator/F2CatalogPlan.h"
#include <atomic>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace PS::F2ReferenceIndex {
struct Result {
    std::vector<F2Catalog::Candidate> entries;
    std::string revision,error;
    unsigned requests=0;
};
// This worker owns copied strings only. It never calls Unreal, loads assets, or writes files.
class Job {
    std::future<Result> future;
    std::shared_ptr<std::atomic<bool>> cancel;
public:
    ~Job(){Shutdown();}
    bool Begin(F2Catalog::Sources sources);
    bool Running()const{return future.valid();}
    std::optional<Result> Poll();
    void Cancel() noexcept;
    void Shutdown() noexcept;
};
} // namespace PS::F2ReferenceIndex
