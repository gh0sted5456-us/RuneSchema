#pragma once
#include "Core/HelpyStorage.h"
#include "Core/HelpyScrap.h"
#include "Runtime/HelpySettings.h"
namespace PS::HelpyInventoryService {
using Json=nlohmann::json;
inline constexpr uint32_t MutationReplyBytes=4096;
inline bool Mutating(const std::string& action){return action=="PrepareDelete"||action=="ObserveDelete"||action=="BeginRestore"||action=="FinishRestore"||action=="Mark"||action=="BeginScrap"||action=="ObserveScrap"||action=="FinishScrap";}
inline Json Run(const std::filesystem::path& directory,const Json& request){
    const auto action=request.at("Action").get<std::string>();
    if(action=="Capabilities")return {{"StorageFormat",1},{"ScrapPlanner",true},{"NativeDeleteCapture",false},{"NativeRestore",false},{"NativeScrap",false}};
    if(action=="Marks"||action=="Mark"){
        const auto& item=request.at("Item");const auto stack=HelpyPersonalization::StackKey(item),type=HelpyPersonalization::ItemKey(item);
        if(stack.empty()||type.empty())throw std::runtime_error("Stable character/item identity required");
        if(action=="Mark"){
            const auto scope=request.at("Scope").get<std::string>();if(scope!="Stack"&&scope!="Type")throw std::runtime_error("Select stack or item-type scope");
            const auto key=scope=="Stack"?stack:type;const auto field=request.at("Field").get<std::string>();
            if(!request.at("Value").is_boolean()||(field!="Favorite"&&field!="Locked"&&field!="Junk"))throw std::runtime_error("Invalid inventory mark");
            Json patch;if(field=="Junk"){auto tags=(scope=="Stack"?HelpyPersonalization::Effective(item):HelpyPersonalization::Get(type)).value("Tags",Json::array());Json next=Json::array();
                for(const auto& tag:tags)if(HelpyPersonalization::Fold(tag.get<std::string>())!="junk")next.push_back(tag);if(request["Value"].get<bool>())next.push_back("junk");patch={{"Tags",next}};
            }else patch={{field,request["Value"]}};
            if(!HelpyPersonalization::Set(key,patch))throw std::runtime_error(HelpyPersonalization::Status);
        }
        return {{"Marks",HelpyPersonalization::Effective(item)},{"Revision",HelpyPersonalization::Revision.load()}};
    }
    if(action=="ScrapPlan"){
        const auto& values=request.at("Recipes");if(!values.is_array()||values.size()>256)throw std::runtime_error("Invalid recipe candidate set");
        std::vector<HelpyScrap::Recipe> recipes;
        const auto amounts=[](const Json& rows){if(!rows.is_array()||rows.size()>32)throw std::runtime_error("Invalid recipe side");std::vector<HelpyScrap::Amount> result;
            for(const auto& row:rows){if(!row.at("Count").is_number_integer())throw std::runtime_error("Invalid native recipe count");result.push_back({row.at("Path").get<std::string>(),row.at("Count").get<int64_t>()});}return result;};
        for(const auto& value:values){if(!value.at("Deterministic").is_boolean())throw std::runtime_error("Invalid recipe verification flag");
            recipes.push_back({value.at("Path").get<std::string>(),amounts(value.at("Inputs")),amounts(value.at("Outputs")),value.at("Deterministic").get<bool>()});}
        if(!request.at("Quantity").is_number_integer())throw std::runtime_error("Invalid scrap quantity");
        return {{"Plan",HelpyScrap::Plan(request.at("Item"),request.at("Quantity").get<int64_t>(),recipes)}};
    }
    HelpyStorage::Store store(directory/"storage",request.at("CharacterGuid").get<std::string>());
    if(action=="StorageRead")return store.Rows();
    if(action=="BeginScrap"){
        if(!HelpySettings::ScrapRefunds())throw std::runtime_error("Enable Scrap refunds in Home quick settings first");
        const auto& plan=request.at("Plan");const Json item={{"Path",plan.at("Path")},{"Instance",plan.at("Instance")},{"PlayerGuid",plan.at("PlayerGuid")},{"Count",plan.at("Quantity")}};
        if(!HelpyPersonalization::BulkEligible(item,false))throw std::runtime_error("Unprotect this stack before scrapping");
        store.PrepareScrap(request.at("CaptureId"),request.at("Item"),request.at("Plan"));return {{"Prepared",true}};
    }
    if(action=="ObserveScrap"){const auto result=request.at("Result").get<std::string>();if(result!="Removed"&&result!="Retained"&&result!="Unknown")throw std::runtime_error("Invalid native scrap deletion result");
        store.ObserveScrap(request.at("CaptureId"),result=="Removed"?HelpyStorage::DeleteResult::Removed:result=="Retained"?HelpyStorage::DeleteResult::Retained:HelpyStorage::DeleteResult::Unknown);return {{"Observed",true}};}
    if(action=="FinishScrap"){
        if(!request.at("Confirmed").is_boolean())throw std::runtime_error("Invalid scrap refund receipt");
        store.FinishScrap(request.at("CaptureId"),request.at("Confirmed"),request.at("Drops"));return {{"Observed",true}};
    }
    if(action=="PrepareDelete"){store.PrepareDelete(request.at("CaptureId"),request.at("Item"));return {{"Captured",true}};}
    if(action=="ObserveDelete"){const auto result=request.at("Result").get<std::string>();if(result!="Removed"&&result!="Retained"&&result!="Unknown")throw std::runtime_error("Invalid native delete result");
        store.ObserveDelete(request.at("CaptureId"),result=="Removed"?HelpyStorage::DeleteResult::Removed:result=="Retained"?HelpyStorage::DeleteResult::Retained:HelpyStorage::DeleteResult::Unknown);return {{"Observed",true}};}
    if(action=="BeginRestore"){const auto document=store.BeginRestore(request.at("StorageId"),request.at("Attempt"),request.at("Revision"),request.at("AlreadyPresent"));return {{"Prepared",true},{"Revision",document.at("Revision")}};}
    if(action=="FinishRestore"){const auto result=request.at("Result").get<std::string>();if(result!="Confirmed"&&result!="NotAttempted"&&result!="Unknown")throw std::runtime_error("Invalid native restore result");
        store.FinishRestore(request.at("StorageId"),request.at("Attempt"),result=="Confirmed"?HelpyStorage::RestoreResult::Confirmed:result=="NotAttempted"?HelpyStorage::RestoreResult::NotAttempted:HelpyStorage::RestoreResult::Unknown);return {{"Observed",true}};}
    throw std::runtime_error("Unknown Helpy inventory operation");
}
}
