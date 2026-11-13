#pragma once
// Renderer-independent inventory delivery contract shared by Helpy's native service.
#include <exception>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace PS::HelpyGrants {
inline constexpr int MaxSelection=64,MaxItemQuantity=10000;
struct Grant { std::string item; int count=1; };
inline void ValidateGrants(const std::vector<Grant>& grants,int maximumItemCount=0) {
    if(grants.empty() || grants.size()>MaxSelection)throw std::runtime_error("Select 1..64 distinct items.");
    std::set<std::string> unique;
    int total=0;
    for(const auto& g:grants){
        if(g.item.empty() || g.item.front()!='/' || g.item.size()>2048 || g.count<1 || g.count>MaxItemQuantity || !unique.insert(g.item).second)
            throw std::runtime_error("Invalid, duplicate or excessive item request.");
        total+=g.count;
    }
    if(maximumItemCount>0&&total>maximumItemCount)
        throw std::runtime_error("Helpy item grant exceeds the server quantity limit of "+std::to_string(maximumItemCount)+".");
}
// A grant may have changed inventory before confirmation fails. Never implicitly
// retry it or claim an atomic rollback after that failure. Only a positively
// identified rejection BEFORE the inventory write may skip to the next item.
struct Rejected : std::runtime_error {using std::runtime_error::runtime_error;};
struct BatchRow { Grant grant; enum class State { Confirmed, Rejected, Unconfirmed, NotAttempted } state; std::string message; };
template<class Give> std::vector<BatchRow> ExecuteBatch(const std::vector<Grant>& grants,Give&& give) {
    ValidateGrants(grants);std::vector<BatchRow> results;bool stop=false;
    for(const auto& grant:grants) {
        if(stop){results.push_back({grant,BatchRow::State::NotAttempted,"Not attempted after the previous error."});continue;}
        try {give(grant);results.push_back({grant,BatchRow::State::Confirmed,"Inventory delta confirmed."});}
        catch(const Rejected& e){results.push_back({grant,BatchRow::State::Rejected,e.what()});}
        catch(const std::exception& e){results.push_back({grant,BatchRow::State::Unconfirmed,e.what()});stop=true;}
        catch(...){results.push_back({grant,BatchRow::State::Unconfirmed,"Unknown grant error; inspect inventory before retrying."});stop=true;}
    }
    return results;
}
} // namespace PS::HelpyGrants
