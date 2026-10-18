#include "Core/UsmapIndex.h"
#include <iostream>
// Read-only mapping investigation; no game attachment or world/save changes.
int main(int argc,char** argv)try{
    if(argc<3)throw std::runtime_error("Arguments: mapping-file workspace-evidence-directory [type queries...]");
    auto& index=PS::Usmap::Index::Shared();nlohmann::json result=nlohmann::json::object();
    std::vector<std::string> queries;if(argc>3)for(int i=3;i<argc;++i)queries.push_back(argv[i]);else queries={"Lore","Book","JournalComponent","JournalEntry"};
    for(const auto& query:queries){
        const auto found=index.Handle(argv[1],"native-contract-20261017",argv[2],{{"Action","SearchTypes"},{"Query",query},{"Limit",100}});
        result[query]=found;
        if(found.contains("Results"))for(const auto& row:found["Results"]){
            const auto path=row.value("Path",std::string{});if(!path.empty())(void)index.Handle(argv[1],"native-contract-20261017",argv[2],{{"Action","DescribeType"},{"Type",path}});
        }
    }
    std::cout<<result.dump(2)<<std::endl;return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 1;}
