#include "Core/J4SSections.h"
#include <cassert>
#include <iostream>
using namespace PS;
int main(){
    using Json=nlohmann::json;
    assert(J4SSections::ValidOwner("jonesing4space.jbm")&&J4SSections::ValidOwner("a.b-c_d.e"));
    for(const auto* bad:{"","nodot","Upper.case",".lead","trail.","a..b","a.-b","a b.c"})assert(!J4SSections::ValidOwner(bad));
    Json save={{"meta_data",{{"char_name","Brewer"}}},{"J4SSaves",{{"format",1},{"mods",Json::object()},{"sections",{
        {"jonesing4space.jbm",{2,7,"0A0B"}},{"broken.mod",{1,0,"00"}},{"Bad Owner",{1,1,"00"}}}}}}};
    const auto rows=J4SSections::List(save);assert(rows.size()==3);
    size_t valid=0;for(const auto& row:rows)if(row.Valid){++valid;assert(row.Owner=="jonesing4space.jbm"&&row.Schema==2&&row.Revision==7&&row.Bytes==2);}
    assert(valid==1);
    assert(J4SSections::Entry(save,"jonesing4space.jbm")&&!J4SSections::Entry(save,"broken.mod"));
    // Clean, then restore from a backup into a save that has no envelope.
    const auto backup=J4SSections::Backup(save,"decaf","Brewer",{"jonesing4space.jbm"},"2026-11-19T17:00:00Z");
    auto cleaned=J4SSections::WithSection(save,"jonesing4space.jbm",std::nullopt);
    assert(!J4SSections::Entry(cleaned,"jonesing4space.jbm")&&cleaned["J4SSaves"]["sections"].contains("broken.mod"));
    const auto restored=J4SSections::ReadBackup(backup,"decaf");
    Json bare={{"meta_data",Json::object()}};
    assert(J4SSections::WithSection(bare,"x.y",std::nullopt)==bare);
    for(const auto& [owner,entry]:restored.items())bare=J4SSections::WithSection(bare,owner,entry);
    assert(bare["J4SSaves"]["format"]==1&&J4SSections::Entry(bare,"jonesing4space.jbm")==Json({2,7,"0A0B"}));
    bool refused=false;try{(void)J4SSections::ReadBackup(backup,"other");}catch(const std::exception&){refused=true;}assert(refused);
    refused=false;try{(void)J4SSections::WithSection(save,"x.y",Json({1,1,"zz"}));}catch(const std::exception&){refused=true;}assert(refused);
    refused=false;try{(void)J4SSections::Backup(save,"decaf","",{"broken.mod"},"t");}catch(const std::exception&){refused=true;}assert(refused);
    assert(J4SSections::BackupName("2026-11-19T17:00:00Z",1)=="20261119T170000Z-1-section.json");
    std::cout<<"j4s-sections passed\n";
}
