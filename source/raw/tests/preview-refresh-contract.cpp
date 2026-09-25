#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("PlayerGhost source is required");
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) throw std::runtime_error("PlayerGhost source is unavailable");
    const std::string source{std::istreambuf_iterator<char>(input), {}};
    const auto require=[](bool value,const char* message) {
        if(!value)throw std::runtime_error(message);
    };
    require(source.find("if(previewBootstrapComplete)return;") == std::string::npos,
        "Preview refresh still stops permanently after initial discovery");
    require(source.find("if(++cadence%15!=0)return;") != std::string::npos,
        "Bounded menu preview refresh cadence is missing");
    require(source.find("Character preview native refresh unavailable; using the bounded menu fallback") != std::string::npos,
        "Preview fallback does not survive a native hook mismatch");
    require(source.find("if(itemSlots.empty())ReleasePreviewLifecycle();") != std::string::npos,
        "Preview polling is not released when item effects are absent");
    std::cout << "Character preview keeps a bounded storefront-neutral refresh fallback.\n";
}
