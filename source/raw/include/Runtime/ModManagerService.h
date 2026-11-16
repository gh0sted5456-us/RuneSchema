#pragma once
#include "Core/ModManager.h"
#include "Core/ModArchiveImport.h"
#include "Runtime/HostServices.h"
#include "Utility/Config.h"
#include "Core/RuntimeModPath.h"
#include "Runtime/Storefront.h"
#include "Core/RegistryEnrollment.h"
#include "Runtime/ModHealthService.h"
#include <chrono>

namespace PS::ModManager {
inline Json Service(const Json& request){
    try{const auto action=request.at("Action").get<std::string>();
        const auto root=RuntimeModPath::Resolve(HostServices::ModDirectory());Safe(root);Json result;
        if(action=="ModsRead"){std::lock_guard lock(Gate);result=Read(root,PSConfig::Get()->GetLoadOrderSettings().enabled);}
        else if(action=="ModsSave"){RuntimeModPath::Expected(root,request.value("ExpectedRoot",std::string{}));result=Save(root,PSConfig::Get()->GetLoadOrderSettings().enabled,request);}
        else if(action=="ModsImport")result=Import(request,root,PSConfig::Get()->GetLoadOrderSettings().enabled);
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
