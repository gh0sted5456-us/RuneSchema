void DragonWildsSpawnLoader::PumpItemIcons() {
    if(!PS::ItemIconRequests::HasPending())return;
    const bool worldReady=m_readyWorld&&IsWorldStillLoaded(m_readyWorld);PS::ItemIconRequests::SetAvailable(worldReady);if(!worldReady)return;
    static uint64_t iconRevision=1;
    for(int job=0;job<4&&PS::ItemIconRequests::HasPending();++job) {
        auto iconRequest=PS::ItemIconRequests::Take();if(!iconRequest)break;const auto icon=iconRequest->value("Icon",std::string{});
        PS::HelpyIconTrace::Event("core-received",icon,"worldReady=true");
        nlohmann::json result;bool cacheHit=false;
        try {
            if(icon.empty())throw std::runtime_error("Item icon request did not contain an icon path");
            result=PS::ItemIconThumbnail::Render(m_readyWorld,icon);cacheHit=result.value("CacheHit",false);
            PS::HelpyIconTrace::Event("core-render-ready",icon,"cacheHit="+(cacheHit?std::string("true"):std::string("false"))+" file='"+result.value("File",std::string{})+"'");
        } catch(const std::exception& error) {
            result={{"Icon",icon},{"Status","Error"},{"Error",error.what()}};
            PS::HelpyIconTrace::Event("core-render-error",icon,"error='"+std::string(error.what())+"'");
            static std::set<std::string> diagnosed;
            if(diagnosed.insert(icon).second)
                PS::Log<LogLevel::Warning>(STR("Helpy icon resolver: '{}' -> {}\n"),PS::ToWideSafe(icon.c_str()),PS::ToWideSafe(error.what()));
        }
        PS::ItemIconRequests::Publish(result);
        if(PS::SpawnToolRequests::PushUpdate)PS::SpawnToolRequests::PushUpdate("icon",result,++iconRevision);
        if(!cacheHit)break;
    }
}
