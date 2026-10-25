#include "Core/RuneSchemaUpdateInfo.h"
#include "Core/SteamNewsInfo.h"
#include <cassert>
using namespace PS::RuneSchemaUpdateInfo;
int main(){
    assert(Parse("0.7.8.0-alpha")&&Parse("v0.8.0")&&Parse("RuneSchema-0.7.8.0-alpha")&&Parse("0.7.7.9m"));
    assert(!Parse("")&&!Parse("latest")&&!Parse("1..2")&&!Parse("1.2.3.4.5")&&!Parse("1.2/<script>")&&!Parse("999999999999.2"));
    assert(Compare("0.7.8.0-alpha","0.7.7.9m")==Comparison::Ahead);
    assert(Compare("0.7.8.0-alpha","0.7.8.0")==Comparison::Newer);
    assert(Compare("0.7.8.0","0.7.8.0-alpha")==Comparison::Ahead);
    assert(Compare("0.7.8.0-alpha.2","0.7.8.0-alpha.10")==Comparison::Newer);
    assert(Compare("0.7.8.0-beta","0.7.8.0-alpha")==Comparison::Ahead);
    assert(Compare("0.7.7.9e","0.7.7.9m")==Comparison::DifferentBuild);
    assert(Compare("0.7.8.0+local","v0.7.8.0+release")==Comparison::Current);
    assert(Compare("0.7.8.0","")==Comparison::Unknown);
    assert(Compare("0.7.8.0","0.8.0")==Comparison::Newer);
    assert(Parse("v0.8.0.0")&&Parse("RuneSchema-0.8.0.0"));
    assert(Compare("0.7.8.0-alpha","0.8.0.0")==Comparison::Newer);
    assert(Compare("0.8.0.0","0.8.0.0")==Comparison::Current);
    assert(Parse("v0.8.0.1b")&&Parse("RuneSchema-0.8.0.1b"));
    assert(Compare("0.8.0.1","0.8.0.1b")==Comparison::Newer);
    assert(Compare("0.8.0.1b","0.8.0.1")==Comparison::Ahead);
    assert(Compare("0.8.0.1b","0.8.0.1b")==Comparison::Current);
    assert(Compare("0.8.0.1b","0.8.0.2")==Comparison::Newer);
    assert(Compare("0.8.0.1b","0.8.0.1e")==Comparison::DifferentBuild);
    assert(MainVersion("inline constexpr char Version[] = \"0.7.7.9m\";\n")=="0.7.7.9m");
    assert(!MainVersion("<html>Version: latest</html>")&&!MainVersion("char Version[] = \"oops\";"));
    const auto post=[](const char* title,long long date){return nlohmann::json{{"title",title},{"date",date},{"url","https://steamcommunity.com/games/1374490/announcements/detail/123"},{"feedname","steam_community_announcements"}};};
    auto news=nlohmann::json{{"appnews",{{"appid",1374490},{"newsitems",nlohmann::json::array({post("Patch 1.2",1000),post("Merchandise sale",2000),post("Hotfix 1.3",1500)})}}}};
    assert(PS::SteamNewsInfo::LatestUpdate(news,3000)->Date==1500);
    news["appnews"]["newsitems"][2]["url"]="https://steamstore-a.akamaihd.net/news/externalpost/steam_community_announcements/1234";assert(PS::SteamNewsInfo::LatestUpdate(news,3000)->Date==1500);
    news["appnews"]["newsitems"][2]["url"]="https://evil.test/";assert(PS::SteamNewsInfo::LatestUpdate(news,3000)->Date==1000);
    news["appnews"]["newsitems"][0]["date"]=999999;assert(!PS::SteamNewsInfo::LatestUpdate(news,3000));
    news["appnews"]["appid"]=1;assert(!PS::SteamNewsInfo::LatestUpdate(news,3000));
}
