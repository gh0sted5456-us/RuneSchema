#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("Contract input unavailable");return {std::istreambuf_iterator<char>(f),{}};}
static void Require(bool value){if(!value)throw std::runtime_error("Alpha UI/load contract regression");}
int main(int argc,char** argv){
    Require(argc==8);
    const auto picker=Read(argv[1]),panel=Read(argv[2]),window=Read(argv[3]),lazy=Read(argv[4]),prefetch=Read(argv[5]),recipe=Read(argv[6]),main=Read(argv[7]);
    Require(picker.contains("RS_ITEM_ROW") && picker.contains("RS_CART_PATH") && picker.contains("BeginDragDropSource"));
    Require(picker.contains("AddToCart(row)") && !picker.contains("if(quickActions)PublishSharedCart(state)"));
    Require(panel.contains("AcceptDragDropPayload(\"RS_ITEM_ROW\")") && panel.contains("RenderCartDock();"));
    Require(panel.contains("SharedCart.Payload(mode)") && panel.contains("SetQuantity(entry.Path,quantity)"));
    Require(!window.contains("helpy-cart-slot") && window.contains("LazyCatalog::PumpBackendSignal()"));
    Require(lazy.contains("std::atomic<uint64_t> BackendGeneration") && lazy.contains("memory_order_release") && lazy.contains("memory_order_acquire"));
    const auto signal=lazy.substr(lazy.find("inline void BackendSignal"),lazy.find("inline void PumpBackendSignal")-lazy.find("inline void BackendSignal"));
    Require(!signal.contains("For(") && !signal.contains("RemoteSubmit"));
    Require(prefetch.contains("GetParmsSize()>256") && prefetch.contains("count!=4") && prefetch.contains("offsetof(LatentArgs,CallbackTarget)"));
    Require(prefetch.contains("GetWorld()") && prefetch.contains("FUNC_Static") && !prefetch.contains("std::thread"));
    Require(prefetch.contains("MaxQueuedAssets=512") && main.contains("IsLoaderEnabled(kind))continue"));
    Require(recipe.contains("RefreshNativeRecipeIndex") && recipe.contains("m_recipeNameCounts.Ambiguous") && !recipe.contains("std::count_if(m_recipeDefs"));
    Require(main.find("PrefetchCookedAssets(plan)")<main.find("PrepareReferences();"));
}
