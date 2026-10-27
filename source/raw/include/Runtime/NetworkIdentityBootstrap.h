#pragma once
#include <functional>
#include "Core/NetworkIdentitySync.h"
namespace PS::NetworkIdentityBootstrap {
    using Snapshot = std::function<nlohmann::json()>;
    using Apply = std::function<void(const NetworkIdentitySync::Permutations&)>;
    void Start(Snapshot snapshot, Apply apply);
    void Stop() noexcept;
}
