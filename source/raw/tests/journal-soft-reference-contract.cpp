#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

int main(int argc, char** argv) {
    assert(argc == 5);
    const auto journal = Read(argv[1]);
    assert(journal.find("m_recipeReferenceIndex") != std::string::npos);
    assert(journal.find("m_finalizeJournalSubsystemResolved") != std::string::npos);
    assert(journal.find("m_subCategoryCache") != std::string::npos);
    assert(journal.find("FindObjectByClassAndName") == std::string::npos);
    for (int i = 1; i < 4; ++i) {
        const auto source = Read(argv[i]);
        assert(source.find("PS::WeakObject") == std::string::npos);
        assert(source.find("WeakPtr.Get") == std::string::npos);
        assert(source.find(".WeakPtr =") == std::string::npos);
        assert(source.find("->WeakPtr =") == std::string::npos);
    }
}
