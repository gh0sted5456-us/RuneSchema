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
    assert(journal.find("m_recipeService->ResolveReference(owner") != std::string::npos);
    assert(journal.find("ApplyProperties(entry, def.Body, def.Owner)") != std::string::npos);
    assert(journal.find("m_recipeReferenceIndex") != std::string::npos);
    assert(journal.find("m_finalizeJournalSubsystemResolved") != std::string::npos);
    assert(journal.find("TEXT(\"InternalName\"), TEXT(\"PersistenceID\")") != std::string::npos);
    assert(journal.find("addReference(RC::StringType(characters.GetData(), characters.Num() - 1), object)") != std::string::npos);
    assert(journal.find("m_subCategoryCache") != std::string::npos);
    assert(journal.find("JournalPlacement::WithNativeGroupKey") != std::string::npos);
    assert(journal.find("entry->GetFName().ToString()") != std::string::npos);
    const auto grouped=Read(argv[2]);
    assert(grouped.find("const FName nativeKey=entry->GetFName()") != std::string::npos);
    assert(grouped.find("const FName key=nativeKey") != std::string::npos);
    assert(grouped.find("RemoveJournalMapAliases") != std::string::npos);
    assert(journal.find("FindObjectByClassAndName") == std::string::npos);
    for (int i = 1; i < 4; ++i) {
        const auto source = Read(argv[i]);
        // The local generation token does not invoke UE4SS's incompatible
        // object-to-soft-reference converter. Engine FWeakObjectPtr does.
        assert(source.find("FWeakObjectPtr(") == std::string::npos);
        assert(source.find("WeakPtr.Get") == std::string::npos);
        assert(source.find(".WeakPtr =") == std::string::npos);
        assert(source.find("->WeakPtr =") == std::string::npos);
    }
}
