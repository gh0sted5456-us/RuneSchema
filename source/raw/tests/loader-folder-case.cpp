#include "Utility/ModFolderLayout.h"
#include <cassert>
#include <chrono>
#include <filesystem>

int main()
{
    namespace fs=std::filesystem;
    using namespace PS::ModFolderLayout;
    assert(EqualsInsensitive("recipes","Recipes"));
    assert(EqualsInsensitive("ASSETS","assets"));
    assert(!EqualsInsensitive("recipes","recipe"));

    const auto token=std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root=fs::temp_directory_path()/("runeschema-loader-case-"+token);
    fs::create_directories(root/"Recipes");
    const auto resolved=ResolveLoaderDirectory(root,"recipes");
    assert(resolved&&resolved->filename()=="Recipes");
    assert(!ResolveLoaderDirectory(root,"assets"));
    fs::remove_all(root);
}
