#pragma once
#include <array>
#include <string_view>

namespace PS::StructuredDiagnosticAdvice {
struct Entry {std::string_view Code,Check;};
inline constexpr std::array<Entry,38> Checks{{
    {"RS-BLD-023", "Check the exact cooked BuildingPieceData path, native PersistenceID/InternalName and optional assertions. Keep gameplay in the cooked asset; only AddTo and Unlock are enrollment metadata. World cleanup remains deferred for rejected content; unrelated character lanes can continue."},
    {"RS-ASSET-001", "Check the exact named target and whether its content is mounted or loaded. An unresolved lookup does not, by itself, prove a misspelled path."},
    {"RS-BP-001", "Compare the complete property path with the target Blueprint's current reflected properties and nested structure."},
    {"RS-JSON-001", "Review the reported source file for JSON/JSONC syntax problems. Use line and column evidence only when the parser actually supplied them."},
    {"RS-RAW-001", "Check the requested raw DataTable target and confirm that it exists in the current game content."},
    {"RS-BLD-002", "The named building entry must be an object. Review that entry in the identified building definition file."},
    {"RS-SPAWN-001", "The spawn document root must be an array. Check the named file before retrying."},
    {"RS-IMP-001", "Review the rejected $Imp* directives and the exact parser error. No import was executed by the rejected request."},
    {"RS-IMP-002", "Inspect the destination's Modules.Modules array and Module object-reference layout before retrying the import."},
    {"RS-IMP-008", "An atomic import was blocked during dependency resolution. Review the preceding dependency errors; the destination was not changed by this batch."},
    {"RS-IMP-011", "This was a blocked dry-run plan, not attempted execution. Inspect the dependency and validation errors before running without $ImpDry."},
    {"RS-LOAD-001", "Review the affected loader directory and the original discovery failure before editing mod definitions."},
    {"RS-ASSET-002", "The asset definition root was rejected. Review the file’s top-level JSONC type and structure."},
    {"RS-ASSET-003", "The requested asset field could not be applied. Inspect the exact field path and original rejection; other fields may have succeeded."},
    {"RS-BP-002", "A required Blueprint lifecycle hook was unavailable. Review the reported hook and native build compatibility rather than assuming a property typo."},
    {"RS-BP-003", "This Blueprint change was deferred by the live-reload path. Restart the game to check whether it can apply during startup."},
    {"RS-JSON-004", "The JSONC input exceeded the supported document-size limit. Split definitions into smaller independent files."},
    {"RS-JSON-005", "The section exceeded its supported file-count limit. Reduce the number of files in that section."},
    {"RS-RAW-002", "The resolved raw target was not a usable DataTable object. Confirm the current game class and expected target type."},
    {"RS-RAW-003", "A raw registry plan was rejected. Inspect the planner evidence and the exact requested row or target before retrying."},
    {"RS-REC-001", "The recipe document root was rejected. Check the expected top-level recipe format."},
    {"RS-REC-002", "A recipe entry or its identity/body was invalid. Inspect the named recipe object and required fields."},
    {"RS-BLD-004", "The building Properties value must be an object. Check the named definition rather than changing unrelated building files."},
    {"RS-BLD-005", "A building patch could not find its target. Confirm the exact existing building identity and whether that definition is loaded."},
    {"RS-BLD-007", "Building Requirements must be an array. Check the failing field in the named definition."},
    {"RS-BLD-008", "The building patch attempted a protected managed identity change. Leave the managed identity unchanged."},
    {"RS-BLD-014", "The Base Builder import must contain a pieces array. Check the reported import source and shape."},
    {"RS-SPAWN-002", "The spawn entry or patch declaration was rejected. Inspect the named entry and its allowed fields."},
    {"RS-SPAWN-003", "The spawn patch identity was not found in the loaded spawn registry. Confirm the identity of the existing spawn record."},
    {"RS-SPAWN-004", "Spawn registration failed validation or reported an exception. Review the exact evidence and required fields, including Location when applicable."},
    {"RS-SPAWN-005", "This spawn change requires restart rather than live reload. Do not interpret Deferred as a failed startup registration."},
    {"RS-EQP-006", "Review the identified equipment definition and the original validation error; this entry was rejected."},
    {"RS-GE-004", "The Gameplay Effect definition was deferred until restart. The record does not mean the effect was applied live."},
    {"RS-NIA-001", "The Niagara definition root was rejected. Review the specified source file and expected root structure."},
    {"RS-ENUM-002", "The enum namespace or value is invalid for this loader. Check the exact namespace and enum entry in the source definition."},
    {"RS-STR-003", "The string override document was rejected. Check the file structure before making string-level changes."},
    {"RS-JRN-002", "The journal definition root is invalid. Review the expected top-level structure of the source file."},
    {"RS-CRS-003", "The course patch declaration was missing or malformed. Inspect the named patch identity and source structure."}
}};
inline std::string_view Find(std::string_view code) noexcept {
    for(const auto& entry:Checks)if(entry.Code==code)return entry.Check;
    return {};
}
}
