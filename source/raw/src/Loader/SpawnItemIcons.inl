void DragonWildsSpawnLoader::PumpItemIcons() {
    const bool worldReady = m_readyWorld && IsWorldStillLoaded(m_readyWorld);
    PS::ItemIconRequests::Available = worldReady;
    if (!worldReady) return;

    auto iconRequest = PS::ItemIconRequests::Take();
    if (!iconRequest) return;

    const auto icon = iconRequest->value("Icon", std::string{});
    try {
        if (icon.empty()) throw std::runtime_error("Item icon request did not contain an icon path");
        PS::Log<LogLevel::Verbose>(
            STR("Item-icon subloader rendering thumbnail for '{}'.\n"),
            PS::ToWideSafe(icon.c_str()));
        auto result = PS::ItemIconThumbnail::Render(m_readyWorld, icon);
        PS::ItemIconRequests::Publish(std::move(result));
        PS::Log<LogLevel::Verbose>(
            STR("Item-icon subloader completed '{}'.\n"),
            PS::ToWideSafe(icon.c_str()));
    } catch (const std::exception& error) {
        PS::ItemIconRequests::Publish({{"Icon",icon},{"Status","Error"},{"Error",error.what()}});
        PS::Log<LogLevel::Warning>(
            STR("Item-icon subloader failed for '{}': {}\n"),
            PS::ToWideSafe(icon.c_str()), PS::ToWideSafe(error.what()));
    }
}
