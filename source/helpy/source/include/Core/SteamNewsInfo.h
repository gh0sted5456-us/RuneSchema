#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
namespace PS::SteamNewsInfo {
struct Announcement {std::string Title,Url;std::int64_t Date=0;};
inline std::optional<Announcement> LatestUpdate(const nlohmann::json& document,std::int64_t now){
    const auto app=document.find("appnews");if(app==document.end()||!app->is_object()||app->value("appid",0)!=1374490)return {};
    const auto rows=app->find("newsitems");if(rows==app->end()||!rows->is_array()||rows->size()>100)return {};
    std::optional<Announcement> newest;
    for(const auto& row:*rows){
        if(!row.is_object()||row.value("feedname",std::string{})!="steam_community_announcements")continue;
        const auto title=row.value("title",std::string{}),url=row.value("url",std::string{});
        // Steam's public feed also returns its fixed CDN news redirect. Only
        // accept the official announcement path and a numeric announcement ID.
        const auto cdn=std::string("https://steamstore-a.akamaihd.net/news/externalpost/steam_community_announcements/");
        const bool officialCdn=url.starts_with(cdn)&&url.size()>cdn.size()&&std::all_of(url.begin()+cdn.size(),url.end(),[](unsigned char c){return std::isdigit(c);});
        if(title.empty()||title.size()>256||url.size()>512||(!officialCdn&&!url.starts_with("https://steamcommunity.com/games/1374490/announcements/detail/")&&!url.starts_with("https://store.steampowered.com/news/app/1374490/view/")))continue;
        if(!row.contains("date")||!row["date"].is_number_integer())continue;
        const auto date=row["date"].get<std::int64_t>();if(date<=0||date>now+300)continue;
        auto lower=title;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(lower.find("patch")==lower.npos&&lower.find("hotfix")==lower.npos&&lower.find("update")==lower.npos)continue;
        if(!newest||date>newest->Date)newest=Announcement{title,url,date};
    }
    return newest;
}
}
