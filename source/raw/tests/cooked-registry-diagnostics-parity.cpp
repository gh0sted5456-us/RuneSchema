#include "Core/CookedPakRegistryManifest.h"
#include <cassert>
int main() {
    using namespace PS::CookedPakRegistryManifest;
    Reset();
    assert(!HasPublishedDeclarations());
    Manifest candidate;
    candidate.Owner = "registry-header-parity-test";
    candidate.Source = "test.jsonc";
    candidate.AssetLanes.push_back({"Recipes", {"/Game/RegistryTest.RegistryTest"}});
    PublishBatch({candidate});
    assert(HasPublishedDeclarations());
    Reset();
    assert(!HasPublishedDeclarations());
    return 0;
}
