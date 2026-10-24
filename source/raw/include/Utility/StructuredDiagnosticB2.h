#pragma once
// B2 source-site helper. All fields come from the request's retained provenance;
// this header never inspects live Unreal objects or writes the legacy log.
#include "Utility/StructuredDiagnostics.h"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace PS::StructuredDiagnosticB2 {
struct Provenance {
    std::string ModId;
    std::string File;
    std::string Target;
    std::string OperationId;
};
inline std::string Utf8Path(const std::filesystem::path& path) {
    const auto text=path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()),text.size()};
}
inline bool SameAsciiDirectory(std::string_view left,std::string_view right) noexcept {
    if(left.size()!=right.size())return false;
    for(std::size_t i=0;i<left.size();++i){
        const auto fold=[](unsigned char c) noexcept -> unsigned char {
            return c>='A'&&c<='Z'?static_cast<unsigned char>(c+('a'-'A')):c;
        };
        if(fold(static_cast<unsigned char>(left[i]))!=fold(static_cast<unsigned char>(right[i])))return false;
    }
    return true;
}
inline std::string SourceWithinSection(const std::filesystem::path& file, std::string_view section) {
    const auto normalized=file.lexically_normal();
    for(auto parent=normalized.parent_path();!parent.empty();) {
        if(SameAsciiDirectory(Utf8Path(parent.filename()),section))
            return Utf8Path(normalized.lexically_relative(parent));
        const auto next=parent.parent_path();
        if(next==parent)break;
        parent=next;
    }
    // Only the filename is provable if the section root is not identifiable.
    return Utf8Path(normalized.filename());
}
inline std::string NewOperationId(std::string_view source) {
    static std::atomic<std::uint64_t> sequence{0};
    return std::string(source)+"-"+std::to_string(sequence.fetch_add(1,std::memory_order_relaxed)+1);
}
inline bool Report(std::string code, std::string severity, std::string outcome,
                   std::string source, std::string title, std::string message,
                   const Provenance& provenance, std::string phase,
                   std::string property={}) noexcept {
    try {
        StructuredDiagnostic::Record record;
        record.Code=std::move(code);
        record.Severity=std::move(severity);
        record.Outcome=std::move(outcome);
        record.Source=std::move(source);
        record.Title=std::move(title);
        record.Message=std::move(message);
        record.OperationId=provenance.OperationId;
        if(!provenance.ModId.empty()) record.Context["ModId"]=provenance.ModId;
        if(!provenance.File.empty()) record.Context["File"]=provenance.File;
        if(!provenance.Target.empty()) record.Context["Target"]=provenance.Target;
        if(!property.empty()) record.Context["Property"]=std::move(property);
        if(!phase.empty()) record.Context["Phase"]=std::move(phase);
        return StructuredDiagnostics::Emit(std::move(record));
    }catch(...){return false;}
}
}
