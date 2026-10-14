#include "Loader/JournalSaveOwnership.h"
#include <stdexcept>
using namespace DragonWilds::JournalSave;
int main() {
    const auto require=[](bool ok){if(!ok)throw std::runtime_error("Journal persistence regression");};
    const auto rejects=[&](const auto& action){bool threw=false;try{action();}catch(const std::exception&){threw=true;}require(threw);};
    const Json original={{"UnlockedEntries",{"vanilla","armor","story","other"}},
        {"UnreadEntries",{"story","other"}},{"UnknownNativeField",42}};
    const auto owned=Record(original,{{"armor","ArmorCollection"},{"story","StoneStory"}});
    require(original.size()==3);
    require(RemoveAbsent(owned,{},true).Journal==owned);
    const auto removed=RemoveAbsent(owned,{"StoneStory"},true);
    require(removed.Removed==std::set<std::string>{"story"});
    require(removed.Journal.at("UnlockedEntries")==Json::array({"vanilla","armor","other"}));
    require(removed.Journal.at("UnreadEntries")==Json::array({"other"}));
    require(removed.Journal.at("UnknownNativeField")==42);
    require(ReadOwnership(removed.Journal)==Owners{{"armor","ArmorCollection"}});
    require(RemoveAbsent(original,{"StoneStory"},true).Journal==original);
    require(Record(owned,{{"armor","ArmorCollection"}})==owned);
    const auto ownership=ReadOwnership(owned);
    require(DecodeNative(EncodeNative(ownership))==ownership);
    require(DecodeNative({}).empty());
    require(EncodeNative({}).empty());
    rejects([&]{DecodeNative({"{}","{}"});});
    rejects([&]{DecodeNative({"not json"});});
    rejects([&]{DecodeNative({"{}"});});
    rejects([&]{DecodeNative({std::string(1024*1024+1,'x')});});
    rejects([&]{Record(owned,{{"armor","DifferentMod"}});});
    rejects([&]{RemoveAbsent(owned,{"StoneStory"},false);});
    rejects([&]{RemoveAbsent(owned,{"../StoneStory"},true);});
    auto malformed=owned;malformed[Manifest]["Version"]=2;
    rejects([&]{RemoveAbsent(malformed,{"StoneStory"},true);});
    malformed=owned;malformed[Manifest]["Entries"].push_back(malformed[Manifest]["Entries"][0]);
    rejects([&]{ReadOwnership(malformed);});
    malformed=owned;malformed["UnreadEntries"]=Json::array({17});
    rejects([&]{RemoveAbsent(malformed,{"StoneStory"},true);});
    const auto empty=RemoveAbsent(Record(Json::object(),{{"story","StoneStory"}}),{"StoneStory"},true);
    require(empty.Journal==Json::object());
    require(RemoveAbsent(removed.Journal,{"StoneStory"},true).Journal==removed.Journal);
    const auto exact=RemoveOwnedIds(owned,{{"armor","ArmorCollection"}});
    require(exact.Removed==std::set<std::string>{"armor"});
    require(exact.Journal.at("UnlockedEntries")==Json::array({"vanilla","story","other"}));
    require(ReadOwnership(exact.Journal)==Owners{{"story","StoneStory"}});
    require(RemoveOwnedIds(owned,{{"armor","DifferentMod"}}).Journal==owned);
    const auto transient=RemoveCurrent(owned,{{"story","StoneStory"}});
    require(transient.at("UnlockedEntries")==Json::array({"vanilla","armor","other"}));
    require(transient.at("UnreadEntries")==Json::array({"other"}));
    require(ReadOwnership(transient)==Owners{{"armor","ArmorCollection"}});
    require(IsSaveTransient(true,true));
    require(!IsSaveTransient(true,false));
    require(!IsSaveTransient(false,true));
    require(!IsSaveTransient(false,false));
    const Owners registered={{"cooked","Dye"},{"earned","Dye"},{"automatic","Dye"}};
    const auto onlyTransient=SelectTransient(registered,{"automatic"});
    require(onlyTransient==Owners{{"automatic","Dye"}});
    const Json progress={{"UnlockedEntries",{"vanilla","cooked","earned","automatic"}},
        {"UnreadEntries",{"cooked","earned","automatic"}},{"FutureNativeField",42}};
    const auto saved=RemoveCurrent(progress,onlyTransient);
    require(saved.at("UnlockedEntries")==Json::array({"vanilla","cooked","earned"}));
    require(saved.at("UnreadEntries")==Json::array({"cooked","earned"}));
    require(saved.at("FutureNativeField")==42);
    require(RemoveCurrent(saved,onlyTransient)==saved);
    require(RemoveCurrent(progress,SelectTransient(registered,{}))==progress);
    rejects([&]{SelectTransient(registered,{"unregistered"});});
    const NativeFields before={std::vector<std::string>{"vanilla","story"},{"story"},EncodeNative({{"story","StoneStory"}})};
    const NativeFields after={std::vector<std::string>{"vanilla"},{},{}};
    for(size_t failure=0;failure<3;++failure) {
        auto actual=before;bool failed=false;
        rejects([&]{ReplaceNativeFields(before,after,[&](size_t index,const auto& value){
            actual[index]=value;
            if(index==failure && !failed){failed=true;throw std::runtime_error("Injected write failure");}
        });});
        require(failed && actual==before);
    }
    auto actual=before;
    ReplaceNativeFields(before,after,[&](size_t index,const auto& value){actual[index]=value;});
    require(actual==after);
}
