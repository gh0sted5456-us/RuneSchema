#pragma once
#include "Core/ModManager.h"
#include "Core/ModArchiveScript.h"
#include "Runtime/HostServices.h"
#include "Utility/Config.h"
#include "Core/RuntimeModPath.h"
#include "Runtime/Storefront.h"
#include "Core/RegistryEnrollment.h"
#include "Runtime/ModHealthService.h"
#include <chrono>

namespace PS::ModManager {
inline Json Import(const Json& request,const std::filesystem::path& root){
    std::lock_guard lock(Gate);
    if(!PSConfig::Get()->GetLoadOrderSettings().enabled)throw std::runtime_error("Enable RuneSchema load-order handling before importing a mod.");
    RuntimeModPath::Expected(root,request.value("ExpectedRoot",std::string{}));const auto mods=root/"mods";Safe(mods);
    const auto archive=std::filesystem::u8path(request.at("Archive").get<std::string>());Safe(archive);
    if(!std::filesystem::is_regular_file(archive)||std::filesystem::file_size(archive)>2ull*1024*1024*1024)throw std::runtime_error("Choose a ZIP archive below 2 GiB.");
    // Stage outside mods: no loader/watcher can discover an incomplete import.
    const auto jobs=root/"mod-imports";Safe(jobs);std::filesystem::create_directories(jobs);
    const auto job=jobs/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!std::filesystem::create_directory(job))throw std::runtime_error("Cannot reserve an import workspace.");Safe(job);
    ConfigFiles::Write(job/"import.ps1",ArchiveScript);
    ConfigFiles::Write(job/"input.json",Json{{"Archive",archive.string()}}.dump());
    wchar_t windows[32768]{};if(!GetWindowsDirectoryW(windows,32768))throw std::runtime_error("Windows directory unavailable.");
    const auto executable=std::filesystem::path(windows)/"System32/WindowsPowerShell/v1.0/powershell.exe";
    auto command=L"\""+executable.wstring()+L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+(job/"import.ps1").wstring()+L"\"";
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,job.c_str(),&startup,&process))throw std::runtime_error("Could not start the Windows ZIP reader.");
    CloseHandle(process.hThread);const auto wait=WaitForSingleObject(process.hProcess,120000);
    if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hProcess);throw std::runtime_error("ZIP inspection timed out. Staged files were not installed.");}CloseHandle(process.hProcess);
    const auto resultFile=job/"result.json";auto bytes=ConfigFiles::Read(resultFile,16384);if(bytes.starts_with("\xef\xbb\xbf"))bytes.erase(0,3);
    const auto receipt=Json::parse(bytes);if(!receipt.value("Ok",false))throw std::runtime_error(receipt.value("Status",std::string("ZIP inspection failed.")));
    const auto owner=receipt.at("Owner").get<std::string>();if(!Name(owner))throw std::runtime_error("Invalid mod owner.");
    const auto staged=job/owner,target=mods/owner;Safe(staged);Safe(target);
    if(std::filesystem::exists(target))throw std::runtime_error("That mod is already installed. Mounted files will not be overwritten. The inspected replacement remains in mod-imports for offline review.");
    for(const auto& item:std::filesystem::directory_iterator(mods))if(Order::Fold(item.path().filename().string())==Order::Fold(owner))throw std::runtime_error("A case-equivalent mod is already installed.");
    if(!ModFolderLayout::LooksLikeRuneSchemaMod(staged))throw std::runtime_error("The archive has no recognized RuneSchema content or PAK folders.");
    (void)Metadata(staged);
    const auto orderFile=mods/"runeschema.txt";Safe(orderFile);const auto expected=Order::Read(orderFile);auto entries=Order::Parse(expected.Contents);
    std::erase_if(entries,[&](const auto& item){return Order::Fold(item.Name)==Order::Fold(owner);});entries.push_back({owner,false});
    if(expected.Exists)ConfigFiles::Backup(orderFile);
    if(!Order::WriteExpected(orderFile,expected,Order::Serialize(entries,expected,true)))throw std::runtime_error("Mod order changed during import. Staged files were not installed.");
    // Disabled before publication; an atomic same-volume rename exposes only a
    // complete payload. Never replace or delete an installed directory.
    if(!MoveFileExW(staged.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot publish the new mod folder. Its order entry remains safely disabled.");
    auto result=Read(root,true);result["Status"]="Imported "+owner+" (disabled). Enable and apply, then restart. No archive code was executed.";return result;
}
inline Json Service(const Json& request){
    try{const auto action=request.at("Action").get<std::string>();
        const auto root=RuntimeModPath::Resolve(HostServices::ModDirectory());Safe(root);Json result;
        if(action=="ModsRead"){std::lock_guard lock(Gate);result=Read(root,PSConfig::Get()->GetLoadOrderSettings().enabled);}
        else if(action=="ModsSave"){RuntimeModPath::Expected(root,request.value("ExpectedRoot",std::string{}));result=Save(root,PSConfig::Get()->GetLoadOrderSettings().enabled,request);}
        else if(action=="ModsImport")result=Import(request,root);
        else return {{"Ok",false},{"Status","Unknown mod operation."}};
        const auto declarations=RegistryEnrollment::DeclarationSnapshot();
        const auto health=ModHealthService::Read();result["HealthLedger"]=health;
        for(auto& row:result["Rows"]){auto& audit=row["RegistryDeclarations"];audit=Json::array();size_t accepted=0,rejected=0;for(const auto& declaration:declarations)if(Order::Fold(declaration.Owner)==Order::Fold(row.at("Name").get<std::string>())){
            if(declaration.Reason.empty())++accepted;else ++rejected;
            if(audit.size()<128)audit.push_back({{"Source",declaration.Source.substr(0,2048)},{"Accepted",declaration.Reason.empty()},{"Reason",declaration.Reason.substr(0,2048)}});
        }row["AcceptedDeclarations"]=accepted;row["RejectedDeclarations"]=rejected;
        const auto owner=row.at("Name").get<std::string>();if(health.value("Mods",Json::object()).contains(owner))row["Health"]=health.at("Mods").at(owner);}
        result["Root"]=RuntimeModPath::Text(root);result["ModsDirectory"]=RuntimeModPath::Text(root/"mods");result["Runtime"]=Storefront::Name(Storefront::Current());return result;
    }catch(const std::exception& error){return {{"Ok",false},{"Status",error.what()}};}
    return {{"Ok",false},{"Status","Unknown mod operation."}};
}
}
