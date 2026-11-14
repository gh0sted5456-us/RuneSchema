#pragma once

namespace PS::PakLoader {
enum class Result : int { Ready, InvalidConfig, NoPaks, InvalidContainers, MountUnavailable };
inline constexpr auto QueryExport = "RuneSchemaPakLoader_QueryV1";
inline constexpr auto StartExport = "RuneSchemaPakLoader_StartV1";
inline constexpr auto CoreRegisterExport = "RuneSchema_RegisterPakLoaderRootV1";
using Query = Result (*)() noexcept;
using Start = Result (*)() noexcept;
using CoreRegister = bool (*)(const wchar_t*) noexcept;
}
