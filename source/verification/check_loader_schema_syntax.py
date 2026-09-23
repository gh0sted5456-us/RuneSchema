"""Compile the shipped LoaderSchemas.h syntax, including nested metadata initializers.

Default mode uses a declarations-only JSON stand-in and schema-constant stand-ins:
this checks C++ parsing, NOT nlohmann behavior, generated schema content or Unreal.
Pass --json-include <directory containing nlohmann/json.hpp> to use real nlohmann.
No downloads, writes to the project, or changes to the user's BAT are performed.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import tempfile
from pathlib import Path

JSON_DECLARATIONS = r'''#pragma once
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
namespace nlohmann {
struct json {
    json();
    json(std::initializer_list<json>);
    template<class T> json(const T&);
    static json object();
    static json array(std::initializer_list<json> = {});
    template<class Key> json& operator[](const Key&);
    template<class Key> const json& operator[](const Key&) const;
    template<class Key> bool contains(const Key&) const;
    void push_back(const json&);
    template<class Key> void erase(const Key&);
    std::map<std::string, json>& items();
};
}
'''
CAPABILITY_DECLARATIONS = r'''#pragma once
#include <string>
namespace PS::JsonSchemaGenerator {
struct SchemaTestCapability {
    bool StarterPatch{}, Clone{}, Append{}, AuthoredSearch{};
};
const SchemaTestCapability& Capability(const std::string&);
}
'''
HUMAN_DECLARATIONS = r'''#pragma once
#include <array>
namespace DragonWilds::HumanNpc {
inline constexpr std::array<const char*, 1> AppearanceKeys{"SyntaxFixture"};
inline constexpr std::array<const char*, 1> EquipmentKeys{"SyntaxFixture"};
}
namespace DragonWilds::HumanPose {
enum class Playback { Hold, Loop, Once };
struct SchemaTestPose { const char* Name; Playback Mode; };
inline constexpr std::array<SchemaTestPose, 1> Presets{{{"SyntaxFixture", Playback::Hold}}};
}
'''


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--json-include', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    header = root / 'raw/include/Generator/LoaderSchemas.h'
    source = header.read_text(encoding='utf-8')
    match = re.search(r'const json authorFlag\s*=\s*(.*?);', source, re.S)
    if not match:
        raise RuntimeError('authorFlag initializer was not found in the shipped header')
    initializer = match.group(1)
    closing = initializer.rfind('}')
    if closing < 0:
        raise RuntimeError('authorFlag initializer has no closing brace')
    # Remove the last enclosing brace to recreate the reported regression.
    broken_initializer = initializer[:closing] + initializer[closing + 1:]
    broken = source[:match.start(1)] + broken_initializer + source[match.end(1):]
    if args.json_include and not (args.json_include / 'nlohmann/json.hpp').is_file():
        raise RuntimeError('--json-include must contain nlohmann/json.hpp')

    with tempfile.TemporaryDirectory(prefix='runeschema-schema-syntax-') as tmp:
        folder = Path(tmp)
        fixture = folder / 'fixtures'
        for part in ['nlohmann', 'Generator', 'Loader']:
            (fixture / part).mkdir(parents=True, exist_ok=True)
        if not args.json_include:
            (fixture / 'nlohmann/json.hpp').write_text(JSON_DECLARATIONS, encoding='utf-8')
        (fixture / 'Generator/LoaderCapabilities.h').write_text(CAPABILITY_DECLARATIONS, encoding='utf-8')
        (fixture / 'Loader/HumanNpc.h').write_text(HUMAN_DECLARATIONS, encoding='utf-8')
        flags = ['-std=c++23', '-Wall', '-Wextra', '-Werror', '-pedantic', '-fsyntax-only']
        if args.json_include:
            flags += ['-I' + str(args.json_include.resolve())]
        flags += ['-I' + str(fixture)]
        for label, body, should_compile in [('negative-control', broken, False), ('fixed-header', source, True)]:
            test_header = folder / (label + '.h')
            test_header.write_text(body, encoding='utf-8')
            unit = folder / (label + '.cpp')
            unit.write_text('#include "' + test_header.name + '"\n', encoding='utf-8')
            run = subprocess.run([args.compiler, *flags, str(unit)], text=True, capture_output=True)
            if should_compile != (run.returncode == 0):
                raise RuntimeError(f'{label}: unexpected compiler result {run.returncode}\n{run.stdout}\n{run.stderr}')
            print(f'{label}: PASS (compiler exit {run.returncode})')
            if not should_compile:
                diagnostics = run.stderr.splitlines()
                print('\n'.join(diagnostics[:8]))
        mode = 'real nlohmann header' if args.json_include else 'declarations-only JSON stand-in'
        print(f'LoaderSchemas full-header syntax: PASS ({mode}; schema-constant stand-ins).')
        print('Not a Windows/UE4SS build or a JSON-schema behavior test.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
