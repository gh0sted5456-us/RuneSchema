void DragonWildsSpawnLoader::PumpItemIcons() {
    if(m_activeItemIcon.empty()&&!PS::ItemIconRequests::HasPending())return;
    const bool worldReady=m_readyWorld&&IsWorldStillLoaded(m_readyWorld);PS::ItemIconRequests::SetAvailable(worldReady);
    if(!worldReady){m_activeItemIcon.clear();PS::ItemIconThumbnail::Reset();return;}
    static uint64_t iconRevision=1;
    for(int job=0;job<4;++job) {
        if(m_activeItemIcon.empty()) {
            auto iconRequest=PS::ItemIconRequests::Take();if(!iconRequest)break;
            m_activeItemIcon=iconRequest->value("Icon",std::string{});
        }
        const auto icon=m_activeItemIcon;
        PS::HelpyIconTrace::Event("core-received",icon,"worldReady=true");
        nlohmann::json result;bool cacheHit=false;
        try {
            if(icon.empty())throw std::runtime_error("Item icon request did not contain an icon path");
            result=PS::ItemIconThumbnail::Render(m_readyWorld,icon);cacheHit=result.value("CacheHit",false);
            if(result.value("Status",std::string{})=="Pending")return;
            PS::HelpyIconTrace::Event("core-render-ready",icon,"cacheHit="+(cacheHit?std::string("true"):std::string("false"))+" file='"+result.value("File",std::string{})+"'");
        } catch(const std::exception& error) {
            PS::ItemIconThumbnail::Reset();
            const auto message=std::string(error.what());
            result={{"Icon",icon},{"Status","Error"},{"Error",message},
                {"Retryable",message.find("blank item icon")!=std::string::npos||message.find("warm-up timed out")!=std::string::npos}};
            PS::HelpyIconTrace::Event("core-render-error",icon,"error='"+std::string(error.what())+"'");
            static std::set<std::string> diagnosed;
            if(diagnosed.insert(icon).second)
                PS::Log<LogLevel::Warning>(STR("Helpy icon resolver: '{}' -> {}\n"),PS::ToWideSafe(icon.c_str()),PS::ToWideSafe(error.what()));
        }
        m_activeItemIcon.clear();
        PS::ItemIconRequests::Publish(result);
        if(PS::SpawnToolRequests::PushUpdate)PS::SpawnToolRequests::PushUpdate("icon",result,++iconRevision);
        if(!cacheHit)break;
    }
}
