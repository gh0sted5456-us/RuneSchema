// Run only after start_mods<CppMod>() has returned. Never append to m_mods
// while UE4SS or a C++ constructor is iterating that vector.
for(size_t round=0;round<4096;++round) {
    auto pending=RuneSchemaManagedPackages::Startup.Take();if(pending.empty())break;
    for(auto& request:pending)try {
        if(find_mod_by_name<LuaMod>(request.Name)||find_mod_by_name<CppMod>(request.Name))throw std::runtime_error("mod name already registered");
        bool nativeReady=!request.Native;
        if(request.Native) {
            auto mod=std::make_unique<CppMod>(*this,StringType(request.Name),StringType(request.Root.native()));
            if(!mod->is_installable())throw std::runtime_error("native DLL or lifecycle exports unavailable");
            auto* instance=mod.get();m_mods.emplace_back(std::move(mod));instance->set_installed(true);instance->start_mod();nativeReady=instance->is_started();
            if(!nativeReady)throw std::runtime_error("native start_mod did not produce a running mod");
        }
        if(request.Lua&&nativeReady) {
            auto mod=std::make_unique<LuaMod>(*this,StringType(request.Name),StringType(request.Root.native()));
            if(!mod->is_installable())throw std::runtime_error("Lua package is not installable");
            m_mods.emplace_back(std::move(mod)); // install_lua_mods() runs at the normal program-start phase.
        }
        RuneSchemaManagedPackages::Startup.Confirm(request);
        Output::send(STR("[RuneSchema][UE4SS][REGISTERED] '{}' from '{}' at '{}'; UE4SS owns its lifecycle.\n"),request.Name,request.Owner,request.Root.native());
    }catch(const std::exception& error){Output::send<LogLevel::Warning>(STR("[RuneSchema][UE4SS][REJECTED] '{}': {}\n"),request.Name,ensure_str(error.what()));}
}
RuneSchemaManagedPackages::Startup.Close();
