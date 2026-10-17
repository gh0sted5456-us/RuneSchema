// UE4SS's ordinary Lua start path supplies independent states, ModRef, module
// search paths, config roots and normal C++ on_lua_start/on_lua_stop callbacks.
for(const auto& request:RuneSchemaManagedPackages::Startup.Snapshot())if(request.Lua)try {
    auto* mod=find_mod_by_name<LuaMod>(request.Name,IsInstalled::Yes);
    if(mod&&!mod->is_started())mod->start_mod();
}catch(const std::exception& error){Output::send<LogLevel::Warning>(STR("[RuneSchema][UE4SS][LUA-FAILED] '{}': {}\n"),request.Name,ensure_str(error.what()));}
