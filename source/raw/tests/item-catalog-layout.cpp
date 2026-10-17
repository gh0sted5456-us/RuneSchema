#include "Core/ItemCatalogLayout.h"
#include <cassert>
#include <string>
#include <unordered_set>
int main(){
    PS::ItemCatalogLayout large{{1,2'000'000,40000}};
    assert(large.Total()==2'040'001);
    assert((large.At(0)==std::pair<std::size_t,std::size_t>{0,0}));
    assert((large.At(1)==std::pair<std::size_t,std::size_t>{1,0}));
    assert((large.At(2'000'001)==std::pair<std::size_t,std::size_t>{2,0}));
    // MaxCape can be returned on the first bounded window despite being very
    // late in the global object roster. Ordinary live/mounted coverage remains.
    std::size_t visits=0;bool found=false;
    for(std::size_t cursor=0;cursor<large.Total()&&visits<128;++cursor,++visits){if(large.At(cursor).first==0){found=true;break;}}
    assert(found&&visits==0);
    PS::ItemCatalogLayout catalogue{{1,40000,0}};
    std::unordered_set<std::string> delivered;
    for(std::size_t cursor=0;cursor<catalogue.Total();++cursor){
        const auto [segment,index]=catalogue.At(cursor);
        const auto path=segment==0||index==39999?std::string("MaxCape"):"Item"+std::to_string(index);
        if(segment==1&&path=="MaxCape")continue; // Same live object is not emitted twice.
        assert(delivered.insert(path).second);
    }
    assert(delivered.size()==40000&&delivered.contains("MaxCape")&&delivered.contains("Item32768"));
    bool rejected=false;try{large.At(large.Total());}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    rejected=false;try{PS::ItemCatalogLayout{{1,std::numeric_limits<std::size_t>::max(),0}}.Total();}catch(const std::runtime_error&){rejected=true;}assert(rejected);
}
