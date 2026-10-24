#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Core/NetworkIdentityPlan.h"
#include "Core/NetworkIdentityState.h"

namespace {
void Need(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
}

int main()
{
    using PS::NetworkIdentityPlan::Candidate;
    const std::vector<Candidate> first{
        {"pWgJ", "/Game/Mods/Bard/Earth_01", 0},
        {"Amo_", "/Game/Mods/Bard/Earth_02", 1},
        {"LRUs", "/Game/Mods/Bard/Earth_03", 2}};
    const std::vector<Candidate> second{first[2], first[0], first[1]};
    const auto left = PS::NetworkIdentityPlan::Canonicalize(first, 43);
    const auto right = PS::NetworkIdentityPlan::Canonicalize(second, 43);
    Need(left.size() == 3 && right.size() == 3, "candidate count changed");
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        Need(left[index].Identity == right[index].Identity,
            "peer discovery order changed canonical identity assignment");
        Need(left[index].AssetPath == right[index].AssetPath,
            "peer discovery order changed canonical asset assignment");
        Need(43 + index <= 65535, "preserved prefix overflowed uint16");
    }

    const std::vector<std::string> stable{"native/A", "native/B",
        left[0].AssetPath, left[1].AssetPath, left[2].AssetPath};
    auto changed = stable;
    std::swap(changed[2], changed[3]);
    Need(PS::NetworkIdentityPlan::Fingerprint(stable)
            != PS::NetworkIdentityPlan::Fingerprint(changed),
        "ordered fingerprint did not detect a peer mapping mismatch");

    bool duplicateRejected = false;
    try
    {
        const std::vector<Candidate> duplicate{
            {"same", "/Game/A", 0}, {"same", "/Game/B", 1}};
        (void)PS::NetworkIdentityPlan::Canonicalize(duplicate, 0);
    }
    catch (const std::runtime_error&)
    {
        duplicateRejected = true;
    }
    Need(duplicateRejected, "duplicate persistence identity was accepted");

    const auto rejected=[](const std::vector<Candidate>& rows,
        std::size_t prefix, const std::string& expected) {
        try { (void)PS::NetworkIdentityPlan::Canonicalize(rows,prefix); }
        catch(const std::runtime_error& error) {
            return std::string(error.what()).find(expected)!=std::string::npos;
        }
        return false;
    };
    const auto makeRows=[](std::size_t count,bool registered) {
        std::vector<Candidate> rows; rows.reserve(count);
        for(std::size_t index=0;index<count;++index)
            rows.push_back({"id-"+std::to_string(index),
                "/Game/Test/Asset_"+std::to_string(index),index,registered});
        return rows;
    };
    // Exact item/recipe cardinalities from the reported Game Pass run. Each
    // follow-up pass must validate the same entries without reserving IDs again.
    for(const auto count:{std::size_t{36766},std::size_t{47411}}) {
        auto rows=makeRows(count,true);
        const auto once=PS::NetworkIdentityPlan::Canonicalize(rows,count);
        std::reverse(rows.begin(),rows.end());
        const auto twice=PS::NetworkIdentityPlan::Canonicalize(rows,count);
        Need(once.size()==count&&twice.size()==count,
            "repeat registration counted existing assets as new network IDs");
        for(std::size_t index=0;index<count;++index)
            Need(once[index].Identity==twice[index].Identity
                &&once[index].SourceIndex==twice[index].SourceIndex
                &&twice[index].NetworkRegistered,
                "repeat registration changed canonical order or source membership");
    }
    auto mixed=makeRows(PS::NetworkIdentityPlan::Capacity,false);
    for(std::size_t index=0;index<60000;++index)mixed[index].NetworkRegistered=true;
    const auto full=PS::NetworkIdentityPlan::Canonicalize(mixed,60000);
    Need(full.size()==PS::NetworkIdentityPlan::Capacity,
        "existing entries incorrectly consumed remaining network capacity");
    for(auto& row:mixed)row.NetworkRegistered=true;
    Need(PS::NetworkIdentityPlan::Canonicalize(mixed,mixed.size()).size()==mixed.size(),
        "full but unchanged network registry could not be validated");
    mixed.push_back({"one-too-many","/Game/Test/Extra",mixed.size(),false});
    Need(rejected(mixed,PS::NetworkIdentityPlan::Capacity,"existing=65535, new=1"),
        "genuine overflow was not rejected with accurate counts");
    Need(rejected({},PS::NetworkIdentityPlan::Capacity+1,"uint16 capacity"),
        "oversized native registry prefix was accepted");
    Need(rejected({{"same","/Game/A",0,true},{"same","/Game/B",1,false}},1,"Duplicate"),
        "existing/new duplicate identity bypassed validation");
    Need(rejected({{"same","/Game/A",0,true},{"same","/Game/B",1,true}},2,"Duplicate"),
        "existing/existing duplicate identity bypassed validation");
    Need(rejected({{"","/Game/A",0,true}},1,"incomplete"),
        "existing incomplete candidate was skipped rather than validated");
    Need(rejected({{"known","/Game/A",0,true}},0,"audited registry prefix"),
        "impossible reuse count was accepted");
    // A persistence-map entry without an audited network entry still needs a
    // slot. Only actual network membership can mark the candidate as reused.
    Need(rejected({{"persistence-only","/Game/A",0,false}},
        PS::NetworkIdentityPlan::Capacity,"new=1"),
        "persistence-only asset was treated as an existing network identity");
    Need(PS::NetworkIdentityPlan::Canonicalize({},PS::NetworkIdentityPlan::Capacity).empty(),
        "empty validation pass consumed a network ID");

    PS::NetworkIdentityState::Reset();
    Need(PS::NetworkIdentityState::DeferItemEnrollment(),"startup clients must not append clone IDs before the sorted batch");
    // Both peers must enter the same native prefix even when one creates its
    // two schema clones first. The reported run was 1495 vs 1493, then 38974
    // on both peers; preserving those early client IDs made the tail differ.
    auto startupRows=makeRows(38974-1493,false);
    const auto serverStartup=PS::NetworkIdentityPlan::Canonicalize(startupRows,1493);
    std::reverse(startupRows.begin(),startupRows.end());
    const auto clientStartup=PS::NetworkIdentityPlan::Canonicalize(startupRows,1493);
    for(std::size_t index=0;index<serverStartup.size();++index)
        Need(serverStartup[index].Identity==clientStartup[index].Identity,"deferred startup peers assigned different item IDs");
    PS::NetworkIdentityState::FinishStartupBatch();
    Need(!PS::NetworkIdentityState::DeferItemEnrollment(),"live runtime clones must still enroll immediately after startup");
    const auto emptyState = PS::NetworkIdentityState::Fingerprint();
    PS::NetworkIdentityState::Publish("COMBAT-SPELL", "authority-order-a");
    PS::NetworkIdentityState::PublishOwner("COMBAT-SPELL", "Bard", "notes-a");
    const auto firstState = PS::NetworkIdentityState::Fingerprint();
    PS::NetworkIdentityState::Publish("ITEM", "authority-order-b");
    PS::NetworkIdentityState::PublishOwner("ITEM", "Flintlock", "pistol-a");
    const auto completeState = PS::NetworkIdentityState::Fingerprint();
    Need(emptyState != firstState && firstState != completeState,
        "network identity handshake fingerprint ignored a registry lane");
    const auto owners=PS::NetworkIdentityState::OwnerSnapshot();
    Need(owners.contains("Bard") && owners.contains("Flintlock"),
        "owner-scoped network fingerprints were not retained");
    PS::NetworkIdentityState::Reset();
    std::cout << "network identity plan: PASS\n";
    return EXIT_SUCCESS;
}
