void DragonWildsSpawnLoader::PumpItemIcons(PS::WorkSlice& work) {
    if(PS::HelpySourceCache::Requested.load()&&PS::HelpySourceCache::Poll(PS::HostServices::ModDirectory())){
        const auto& sources=PS::HelpySourceCache::Current;size_t indexed=0,metadata=0,unavailable=0;
        for(const auto& [file,entry]:sources.Containers.items()){
            if(entry.value("Status",std::string{})=="Indexed")++indexed;
            else if(entry.value("Status",std::string{})=="MetadataOnly")++metadata;
            else {++unavailable;PS::Log<LogLevel::Verbose>(STR("[CACHE][PAK-DISCOVERY][UNAVAILABLE] {}: {}\n"),PS::ToWideSafe(file),PS::ToWideSafe(entry.value("Reason",std::string{})));}
        }
        PS::Log<LogLevel::Normal>(STR("[CACHE][PAK-DISCOVERY] {} containers indexed, {} native metadata-only, {} unavailable; {} exact package associations. Live item checks remain required; conflicting mount precedence stays unverified.\n"),
            indexed,metadata,unavailable,sources.ContainerAssets?sources.ContainerAssets->size():0);
    }
    if(!BulkIconJob.Active&&!PS::IconCacheBuild::HasWork()&&m_activeItemIcon.empty()&&!PS::ItemIconRequests::HasPending())return;
    if(!work.MayStart()||PS::SpawnToolRequests::Waiting.load())return;
    work.Started();
    auto* iconWorld=MenuIconWorld(m_readyWorld&&IsWorldStillLoaded(m_readyWorld)?m_readyWorld:nullptr);
    BulkIconJob.Tick(iconWorld,m_activeItemIcon.empty()&&!PS::ItemIconRequests::HasPending());
    if(BulkIconJob.Capturing)return;
    if(m_activeItemIcon.empty()&&!PS::ItemIconRequests::HasPending())return;
    const bool worldReady=iconWorld!=nullptr;PS::ItemIconRequests::SetAvailable(worldReady);
    if(!worldReady){m_activeItemIcon.clear();PS::ItemIconThumbnail::Reset();return;}
    static uint64_t iconRevision=1;
    const auto frameDeadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(2);
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
            result=PS::ItemIconThumbnail::Render(iconWorld,icon);cacheHit=result.value("CacheHit",false);
            if(result.value("Status",std::string{})=="Pending")return;
            PS::HelpyIconTrace::Event("core-render-ready",icon,"cacheHit="+(cacheHit?std::string("true"):std::string("false"))+" file='"+result.value("File",std::string{})+"'");
        } catch(const std::exception& error) {
            PS::ItemIconThumbnail::Reset();
            const auto message=std::string(error.what());
            result={{"Icon",icon},{"Status","Error"},{"Error",message},
                {"Retryable",PS::IconCapture::Retryable(message)}};
            PS::HelpyIconTrace::Event("core-render-error",icon,"error='"+std::string(error.what())+"'");
            static std::set<std::string> diagnosed;
            if(diagnosed.insert(icon).second)
                PS::Log<LogLevel::Warning>(STR("Helpy icon resolver: '{}' -> {}\n"),PS::ToWideSafe(icon.c_str()),PS::ToWideSafe(error.what()));
        }
        m_activeItemIcon.clear();
        PS::ItemIconRequests::Publish(result);
        if(PS::SpawnToolRequests::PushUpdate)PS::SpawnToolRequests::PushUpdate("icon",result,++iconRevision);
        if(!cacheHit||std::chrono::steady_clock::now()>=frameDeadline)break;
    }
}
