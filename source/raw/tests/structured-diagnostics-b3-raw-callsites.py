#!/usr/bin/env python3
"""Compile production RawTableLoader structured-report call expressions with typed stubs.

This specifically catches invalid bool declaration/use and void-cast boolean expressions
without pretending to compile the Unreal/Windows-dependent translation unit.
"""
from pathlib import Path
import argparse
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "src" / "Loader" / "DragonWildsRawTableLoader.cpp"


def extract(text: str, start: str, terminator: str) -> str:
    begin = text.index(start)
    end = text.index(terminator, begin) + len(terminator)
    statement = text[begin:end].strip()
    assert statement.endswith(';'), statement
    return statement


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()
    source = args.source.read_text(encoding='utf-8')
    reload_stmt = extract(source,
        'const bool structured=PS::StructuredDiagnosticB3::Report("RS-RAW-002","Error","Failed",',
        'context,"Reload");')
    origin_stmt = extract(source,
        'anyStructured=PS::StructuredDiagnosticB3::Report("RS-RAW-002","Error","Failed",',
        'provenance,"Resolution")||anyStructured;')

    code = r'''
#include <cassert>
#include <deque>
#include <stdexcept>
#include <string>
namespace PS { namespace StructuredDiagnosticB2 { struct Provenance {}; }
namespace StructuredDiagnosticB3 {
inline std::deque<bool> results;
inline int calls=0;
template<typename ...Args> bool Report(Args&&...) {
    ++calls;
    assert(!results.empty());
    bool result=results.front(); results.pop_front();
    return result;
}
}}
static bool LiveReload() {
    PS::StructuredDiagnosticB2::Provenance context{};
    RELOAD_STATEMENT
    return structured;
}
static bool ApplyAll(int n) {
    bool anyStructured=false;
    std::runtime_error error("not a table");
    for(int i=0;i<n;i++) {
        PS::StructuredDiagnosticB2::Provenance provenance{};
        ORIGIN_STATEMENT
    }
    return anyStructured;
}
int main() {
    using namespace PS::StructuredDiagnosticB3;
    results={false}; calls=0;
    assert(!LiveReload() && calls==1 && results.empty());
    results={true}; calls=0;
    assert(LiveReload() && calls==1 && results.empty());
    results={false,false,true,false}; calls=0;
    assert(ApplyAll(4) && calls==4 && results.empty());
    results={false,false}; calls=0;
    assert(!ApplyAll(2) && calls==2 && results.empty());
}
'''.replace('RELOAD_STATEMENT', reload_stmt).replace('ORIGIN_STATEMENT', origin_stmt)
    with tempfile.TemporaryDirectory(prefix='b3_raw_callsites_') as directory:
        cpp = Path(directory) / 'test.cpp'
        exe = Path(directory) / 'test'
        cpp.write_text(code, encoding='utf-8')
        subprocess.run([args.compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic',
                        str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print('PASS: exact B3 raw reporter expressions type-check and retain every origin')


if __name__ == '__main__':
    main()
