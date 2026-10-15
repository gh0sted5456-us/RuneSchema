#include "Core/HelpyNavigation.h"
#include <stdexcept>
#include <set>
int main(){
    using namespace PS::HelpyNavigation;
    std::set<std::string_view> titles;
    for(auto page:Pages){
        if(!Visible(page)||std::string_view(Description(page)).empty())throw std::runtime_error("Workspace is unreachable or unexplained");
        titles.insert(Title(page));
    }
    const std::set<std::string_view> expected{"Item Service","Transmog & Creation","Recipes","Spawns","Map","Tools & Settings"};
    if(titles!=expected)throw std::runtime_error("All six destinations must stay reachable");
}
