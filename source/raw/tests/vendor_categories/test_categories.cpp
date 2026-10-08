#include "FakeUnreal.h"
#include "Loader/VendorCategoryLabel.h"
#include "Loader/VendorCategoryText.h"
#include <cstddef>
#include <iostream>
#include <string_view>
#include <vector>

using namespace RC::Unreal;
namespace Label = DragonWilds::VendorCategoryLabel;
namespace Text = DragonWilds::VendorCategoryText;
int checks = 0;
void Check(bool okay, const char* what) {
    ++checks;
    if (!okay) throw std::runtime_error(what);
}
template<class F> void Reject(F&& fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "Expected a visible failure, not silent acceptance");
}
struct Backend {
    UObject object;
    FStrProperty inString{L"InString", 0};
    FTextProperty outText;
    FTextProperty inText;
    FStrProperty outString;
    UFunction write;
    UFunction read;
    explicit Backend(int textSize)
      : outText(L"ReturnValue", sizeof(FString), textSize),
        inText(L"InText", 0, textSize), outString(L"ReturnValue", textSize),
        write{{&inString, &outText}, &inString, &outText, int(sizeof(FString))+textSize, true},
        read{{&inText, &outString}, &inText, &outString, textSize+int(sizeof(FString)), false}
    { library = &object; stringToText = &write; textToString = &read; }
    ~Backend() { library = nullptr; stringToText = nullptr; textToString = nullptr; }
};
struct Row {
    FTextProperty property;
    std::vector<std::max_align_t> storage;
    explicit Row(int size) : property(L"Label", 0, size),
        storage((size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t)) {
        property.InitializeValue(storage.data());
    }
    ~Row() { property.DestroyValue(storage.data()); }
    void* Data() { return storage.data(); }
};
int main() {
try {
    const std::vector<std::string> titles = {
        "Weapons", "Armour", "Runes", "Food", "Items", "A", "X", "123", "0",
        "Melee Weapons", "Magic Supplies", "Rune Essence", "VeryLongSingleWordCategoryName",
        "Blacksmith's", "Herbs/Seeds", "Weapons_2", "Tier-3", "A & B", "Potion (Rare)",
        "Food: Fresh", "Quoted \"Items\"", "Path\\Label", "  Armour  ", "Melee  Weapons",
        "None", "INVTEXT", "Équipement", "魔法", "Runes 🜁"
    };
    for (const auto& title : titles)
        Check(Label::Validate(title) == title, "Literal label changed during validation");
    Check(Label::Validate(std::string(128,'x')).size()==128, "128-byte boundary rejected");
    Reject([]{ Label::Validate(""); });
    Reject([]{ Label::Validate(" \t\n\r\f\v"); });
    Reject([]{ Label::Validate(std::string(129,'x')); });
    Reject([]{ Label::Validate(std::string("Food\0Hidden",11)); });
    Reject([]{ Label::RequireExact("Weapons", "weapons"); });
    Reject([]{ Label::RequireExact("Armour", ""); });
    Reject([]{ Label::RequireExact("Melee Weapons", "Melee"); });
    Reject([]{ Label::RequireExact("Food", "Food "); });

    // Run the actual production native bridge against two mocked FText layouts.
    // The tests exercise conversion, reflection checks, ownership and read-back;
    // they do not run Unreal Engine or the store widget.
    for (int size : {16,24}) {
        Backend backend(size);
        {
            Row row(size);
            for (const auto& title : titles) {
                Text::Write(row.Data(), &row.property, title);
                Check(Text::Read(row.Data(), &row.property)==title, "Native round trip changed a label");
                Check(liveTexts==1, "Native conversion leaked/aliased text storage");
                Check(liveStrings==0, "Native conversion leaked string parameters");
            }
            // Copy into another row, then overwrite the original. The clone must
            // retain its independently owned reference after parameter cleanup.
            Text::Write(row.Data(), &row.property, "Weapons");
            {
                Row clone(size);
                clone.property.CopyCompleteValue(clone.Data(), row.Data());
                Text::Write(row.Data(), &row.property, "Armour");
                Check(Text::Read(clone.Data(), &clone.property)=="Weapons", "Copied category lost ownership");
                Check(Text::Read(row.Data(), &row.property)=="Armour", "Original category did not update");
            }
            Check(liveTexts==1, "Cloned category leaked after destruction");

            corruptText = true;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Weapons"); });
            Reject([&]{ Text::Write(row.Data(), &row.property, "Melee Weapons"); });
            corruptText = false;
            Text::Write(row.Data(), &row.property, "Food");
            Check(liveStrings==0, "Failure path leaked FString parameters");

            library = nullptr;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            library = &backend.object;
            stringToText = nullptr;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            stringToText = &backend.write;
            textToString = nullptr;
            Reject([&]{ Text::Read(row.Data(), &row.property); });
            textToString = &backend.read;
            Check(liveStrings==0, "Missing converter leaked parameters");

            const auto savedOffset = backend.outText.offset;
            backend.outText.offset = 0;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            backend.outText.offset = backend.write.paramsSize;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            backend.outText.offset = savedOffset;
            backend.outText.dimension = 2;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            backend.outText.dimension = 1;
            backend.outText.failInitialize = true;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            backend.outText.failInitialize = false;
            Check(liveStrings==0, "Partial initialization leaked input string");
            const auto savedSize=backend.write.paramsSize;
            backend.write.paramsSize=0;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            backend.write.paramsSize=savedSize;
            FStrProperty unexpected{L"Unexpected",0};
            backend.write.fields.push_back(&unexpected);
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            backend.write.fields.pop_back();
            Row wrongSize(size==16?24:16);
            Reject([&]{ Text::Write(wrongSize.Data(), &wrongSize.property, "Food"); });
            Reject([&]{ Text::Read(wrongSize.Data(), &wrongSize.property); });
            row.property.offset = -1;
            Reject([&]{ Text::Write(row.Data(), &row.property, "Food"); });
            Reject([&]{ Text::Read(row.Data(), &row.property); });
            row.property.offset = 0;
            Reject([&]{ Text::Write(nullptr, &row.property, "Food"); });
            Reject([&]{ Text::Read(nullptr, &row.property); });
            Text::Write(row.Data(), &row.property, "Runes");
            Check(Text::Read(row.Data(), &row.property)=="Runes", "Bridge did not recover after rejected calls");
        }
        Check(liveTexts==0, "Text ownership leak after row destruction");
        Check(liveStrings==0, "String ownership leak after row destruction");
    }
    std::cout << "PASS: " << checks << " assertions; " << titles.size()
        << " literal labels; 16/24-byte mocked text layouts; " << conversionCalls
        << " mock native calls.\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
