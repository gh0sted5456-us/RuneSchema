#include "../../shared/NativeServerAdmin.h"
#include <cassert>
#include <functional>
int main(){
    using namespace PS::NativeServerAdmin;
    const auto denied=[](const std::function<void()>& call){try{call();return false;}catch(...){return true;}};
    assert(Operation({{"Operation","Read"}})=="Read");
    assert(Operation({{"Operation","AuthorizeAll"},{"WorldToken","1:2"}})=="AuthorizeAll");
    assert(Operation({{"Operation","AutoAuthorize"},{"WorldToken","1:2"},{"Enabled",true}})=="AutoAuthorize");
    assert(denied([&]{Operation({{"Operation","AutoAuthorize"},{"WorldToken","1:2"},{"Enabled","yes"}});}));
    Json request={{"Operation","Grant"},{"PlayerGuid",std::string(32,'a')},{"TargetToken","41:62"},{"WorldToken","1:2"}};
    assert(Operation(request)=="Grant"&&Guid(std::string(32,'a'))==std::string(32,'A'));
    request["Operation"]="Authorize";assert(Operation(request)=="Authorize");
    request["Operation"]="Unauthorize";assert(Operation(request)=="Unauthorize");
    request["Operation"]="Revoke";assert(Operation(request)=="Revoke");
    request["Role"]="owner";request["PinAuthenticated"]=true;
    assert(denied([&]{Authorize(false,false,true,true);}));
    assert(denied([&]{Authorize(true,true,true,true);}));
    assert(denied([&]{Authorize(true,false,false,true);}));
    assert(denied([&]{Authorize(true,false,true,false);}));
    Authorize(true,false,true,true);
    request["Operation"]="SetOwner";assert(denied([&]{Operation(request);}));
    request["Operation"]="Grant";request["TargetToken"]="../../../other";assert(denied([&]{Operation(request);}));
    request["TargetToken"]="41:62";request["PlayerGuid"]=std::string(32,'0');assert(denied([&]{Operation(request);}));
    request["PlayerGuid"]="same display name";assert(denied([&]{Operation(request);}));
    Json snapshot={{"Kind","RuneSchemaServerControl1"},{"WorldToken","1:2"},{"CanManage",false},{"Reason","Owner required"},{"Players",Json::array({{{"Name","Player"},{"Guid",std::string(32,'a')},{"TargetToken","3:4"},{"Owner",false},{"RoleStatus",{{"NativeKnown",true},{"Role","member"}}}}})}};
    assert(Snapshot(snapshot));snapshot["CanAuthorize"]=true;snapshot["AutoAuthorize"]=false;assert(Snapshot(snapshot));
    snapshot["AutoAuthorize"]="enabled";assert(!Snapshot(snapshot));snapshot["AutoAuthorize"]=true;
    snapshot["Players"][0]["PlatformAccountName"]="Account";snapshot["Players"][0]["Platform"]="Game Pass";snapshot["Players"][0]["Authorized"]=true;assert(Snapshot(snapshot));
    snapshot["CanManage"]="owner";assert(!Snapshot(snapshot));snapshot["CanManage"]=false;
    snapshot["Players"][0]["RoleStatus"]["NativeKnown"]=false;assert(!Snapshot(snapshot));
    snapshot["Players"][0]["RoleStatus"]["Role"]="unknown";assert(Snapshot(snapshot));
    snapshot["Players"][0]["TargetToken"]="3:4:5";assert(!Snapshot(snapshot));
}
