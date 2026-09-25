#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Log-budget source input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

static void Require(const std::string& source, const char* text) {
    if (source.find(text) == std::string::npos)
        throw std::runtime_error(std::string("Bounded main-menu logging is missing: ") + text);
}

int main(int argc, char** argv) {
    if (argc != 5) throw std::runtime_error("Four log-budget source inputs are required");
    const auto recipes = Read(argv[1]);
    const auto assets = Read(argv[2]);
    const auto blueprints = Read(argv[3]);
    const auto mainLoader = Read(argv[4]);
    Require(recipes, "constexpr size_t detailLimit = 12");
    Require(recipes, "additional successful operation detail(s) omitted");
    Require(recipes, "Recipes: {} created, {} edited, {} placed, {} error{}");
    Require(assets, "constexpr size_t detailLimit = 8");
    Require(assets, "additional successful clone detail(s) omitted");
    Require(blueprints, "additional successful change detail(s) omitted");
    Require(blueprints, "Blueprints: {} change set(s) applied");
    Require(mainLoader, "addedPakDirectories");
    std::cout << "Main-menu log budget contract passed.\n";
}
