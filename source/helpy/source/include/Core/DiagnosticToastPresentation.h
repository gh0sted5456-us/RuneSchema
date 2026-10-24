#pragma once
#include "Core/DiagnosticToastFeed.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace PS::DiagnosticToastPresentation {
// Presentation is deliberately separate from the source log. Neither the
// source message nor its severity/history entry is ever rewritten.
struct Display {
    std::string Title;
    std::string Detail;
    std::string Subtitle;
    std::string Level;
    bool Summary = false;
    bool Truncated = false;
    std::uint32_t Repetitions = 1;
    enum class Icon { Information, Diagnostic, Debug, Warning, Error, Activity } Type=Icon::Information;
};
inline std::string_view Trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}
inline bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) return false;
    for (size_t i=0;i<prefix.size();++i)
        if (std::tolower(static_cast<unsigned char>(text[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    return true;
}
inline bool MetadataTag(std::string_view token) {
    constexpr std::string_view names[]={"diagnostic","verbose","debug","warning","warn","error","fatal","info","summary"};
    for(const auto name:names)if(token.size()==name.size() && StartsWithNoCase(token,name))return true;
    return false;
}
inline std::string_view Body(std::string_view raw) {
    // RuneSchema file entries have timestamp/thread prefixes. Skip only the
    // known envelope and severity tokens; unknown [module] tags retain meaning.
    const auto rs = raw.find("[RuneSchema]");
    if (rs != std::string_view::npos) raw.remove_prefix(rs + sizeof("[RuneSchema]")-1);
    raw=Trim(raw);
    for (int i=0;i<4 && raw.starts_with('[');++i) {
        const auto end=raw.find(']');
        if (end==std::string_view::npos || !MetadataTag(raw.substr(1,end-1))) break;
        raw=Trim(raw.substr(end+1));
    }
    return raw;
}
inline bool LimitDetail(std::string& message){
    constexpr size_t maxBytes=160;
    if(message.size()<=maxBytes)return false;
    size_t limit=maxBytes;
    while(limit>0 && (static_cast<unsigned char>(message[limit])&0xC0)==0x80)--limit;
    // Prefer a word break near the end; never truncate the actual log entry.
    const auto space=message.rfind(' ',limit);
    if(space!=std::string::npos && limit-space<28)limit=space;
    message.resize(limit);message+=" ...";
    return true;
}
inline Display Format(const DiagnosticToastFeed::Notice& card) {
    Display result;
    result.Summary=card.Summary;
    result.Repetitions=card.Repetitions;
    result.Type=card.Summary?Display::Icon::Activity:card.Severity==LiveLog::Level::Error?Display::Icon::Error:
        card.Severity==LiveLog::Level::Warning?Display::Icon::Warning:
        card.Severity==LiveLog::Level::Diagnostic?Display::Icon::Diagnostic:Display::Icon::Information;
    result.Level=LiveLog::Label(card.Severity);
    // The existing Live Log models [debug] as Diagnostic severity. Distinguish
    // the explicit marker only for presentation; do not alter filter/history.
    std::string_view envelope=card.Text;
    const auto sourceTag=envelope.find("[RuneSchema]");
    if(sourceTag!=std::string_view::npos){
        envelope=Trim(envelope.substr(sourceTag+sizeof("[RuneSchema]")-1));
        if(StartsWithNoCase(envelope,"[debug]")){
            result.Level="Debug";
            result.Type=Display::Icon::Debug;
        }
    }
    result.Subtitle.clear();
    if (card.Summary) {
        result.Title="More log activity";
        result.Detail=card.Text; // The feed builds this bounded summary.
        result.Subtitle="Full details in HELPY > Diagnostics > Live Log";
        return result;
    }
    if(card.ImportReport){
        result.Title=!card.ImportHasStatus?"Import dry-run report":
            card.ImportReady && card.ImportStats.Errors==0?"Import dry-run ready":"Import dry-run blocked";
        if(card.ImportHasStatus){
            result.Detail="Requested "+std::to_string(card.ImportStats.Requested)+
                " | Resolved "+std::to_string(card.ImportStats.Resolved)+
                " | Planned "+std::to_string(card.ImportStats.Planned)+
                " | Reused "+std::to_string(card.ImportStats.Reused)+
                " | Errors "+std::to_string(card.ImportStats.Errors);
        }else result.Detail="Incomplete import-map report. Review the original log.";
        if(!card.ImportModules.empty())result.Detail+="\nModules: "+card.ImportModules;
        result.Truncated=LimitDetail(result.Detail);
        result.Subtitle="Dry run only · full report in HELPY > Diagnostics > Live Log";
        result.Level=LiveLog::Label(card.Severity);
        return result;
    }
    if(card.BlueprintBatch && (card.BatchCount>1 || card.BlueprintSummary)){
        result.Title=card.BatchErrors?"Blueprint update reported errors":"Blueprint changes applied";
        result.Detail=std::to_string(card.BatchCount)+" Blueprint modifications applied";
        if(card.BatchErrors)result.Detail+="; "+std::to_string(card.BatchErrors)+" errors reported";
        for(const auto& asset:card.BatchAssets)result.Detail+="\n"+asset;
        if(card.BatchCount>card.BatchAssets.size())
            result.Detail+="\n+"+std::to_string(card.BatchCount-card.BatchAssets.size())+" more modifications";
        result.Subtitle="Details in HELPY > Diagnostics > Live Log";
        return result;
    }
    auto body=Body(card.Text);
    constexpr std::string_view blueprint="Applied Blueprint changes to ";
    if (body.starts_with(blueprint)) {
        result.Title="Blueprint changes applied";
        body=Trim(body.substr(blueprint.size()));
        // The full original entry remains in Live Log; trim terminal punctuation
        // only when the remainder looks like a single asset identifier.
        if (!body.empty() && body.back()=='.' && body.find(' ')==std::string_view::npos) body.remove_suffix(1);
        result.Detail=std::string(body);
    } else if(StartsWithNoCase(body,"Auto-reloaded mod ")){
        result.Title="Mod automatically reloaded";
        result.Detail=std::string(Trim(body.substr(sizeof("Auto-reloaded mod ")-1)));
    } else {
        DiagnosticToastPatterns::BlueprintSummary blueprintSummary{};
        std::uint32_t omitted=0;
        if(DiagnosticToastPatterns::ParseBlueprintSummary(card.Text,blueprintSummary)){
            result.Title=blueprintSummary.Errors?"Blueprint loader reported errors":"Blueprint loader completed";
            result.Detail=std::to_string(blueprintSummary.Updated)+" updated | "+std::to_string(blueprintSummary.Errors)+" errors";
        }else if(DiagnosticToastPatterns::ParseBlueprintOmitted(card.Text,omitted)){
            result.Title="Blueprint details omitted";
            result.Detail=std::to_string(omitted)+" additional change details are available in Diagnostics > Live Log";
        }else if(StartsWithNoCase(body,"[loader=assets]")){
            result.Title="Asset loader update";
            result.Detail=std::string(body);
        }else if(StartsWithNoCase(body,"IMPORT MAP")){
            result.Title="Import dry-run report";
            result.Detail=std::string(body);
        }else {
        switch(card.Severity) {
            case LiveLog::Level::Error: result.Title="RuneSchema error";break;
            case LiveLog::Level::Warning: result.Title="RuneSchema warning";break;
            case LiveLog::Level::Diagnostic: result.Title="Diagnostic message";break;
            default:result.Title="RuneSchema message";break;
        }
        result.Detail=std::string(body);
        }
    }
    if (result.Detail.empty()) result.Detail="(No additional message text)";
    result.Truncated=LimitDetail(result.Detail);
    if(result.Truncated)result.Subtitle="Full entry in HELPY > Diagnostics > Live Log";
    return result;
}
}
