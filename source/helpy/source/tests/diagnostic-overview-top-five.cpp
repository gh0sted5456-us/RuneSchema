#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

struct Entry {std::uint64_t Id, LastSeen;bool Viewed, Error;};
static bool Priority(const Entry* a,const Entry* b){
    if(a->Viewed!=b->Viewed)return !a->Viewed;
    if(a->Error!=b->Error)return a->Error;
    return a->LastSeen>b->LastSeen;
}
int main(){
    for(std::size_t n:{0u,1u,2u,4u,5u,6u,25u,4000u}){
        std::vector<Entry> groups;
        for(std::size_t i=0;i<n;++i){
            groups.push_back({i+1,1000000-(i*271u)%999983u,i%3u==0u,i%5u!=0u});
        }
        std::vector<const Entry*> expected,actual;
        for(const auto& group:groups){expected.push_back(&group);actual.push_back(&group);}
        const std::size_t count=std::min<std::size_t>(actual.size(),5);
        std::sort(expected.begin(),expected.end(),Priority);
        std::partial_sort(actual.begin(),actual.begin()+count,actual.end(),Priority);
        for(std::size_t i=0;i<count;++i)assert(actual[i]->Id==expected[i]->Id);
    }
}
