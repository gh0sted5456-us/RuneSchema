#include "Core/MetadataWindow.h"
#include <cassert>
int main(){
    std::vector<int> rows(10000);for(size_t i=0;i<rows.size();++i)rows[i]=int(i);
    const auto counts=PS::CopyMetadataWindow(rows,7,0,0);assert(counts.Total==10000&&counts.Revision==7&&counts.Records.empty());
    size_t visited=0,calls=0;while(visited<rows.size()){
        const auto page=PS::CopyMetadataWindow(rows,7,visited,9999);assert(page.Records.size()<=128&&page.Revision==7);
        for(auto value:page.Records)assert(value==int(visited++));++calls;
    }assert(calls==79&&visited==10000);
    assert(PS::CopyMetadataWindow(rows,8,10001,128).Records.empty());
    assert(PS::CopyMetadataWindow(rows,8,0,128).Revision!=counts.Revision);
}
