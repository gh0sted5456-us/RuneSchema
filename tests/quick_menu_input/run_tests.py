#!/usr/bin/env python3
"""Compile the shipped adapter and exact lifecycle methods with a mock host.

No Unreal SDK or running game is used. These checks do not validate UE4SS ABI,
Windows hook behavior, physical on-screen presentation, or engine ProcessEvent.
The existing QuickMenuUI.h must be present in the target working source.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler', default='g++')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--ui-include', type=Path, help='Optional existing raw/include path for testing the partial overlay')
    args = p.parse_args()
    tests = Path(__file__).resolve().parent
    raw = tests.parents[1]
    source = (raw/'src/Generator/InGameQuickMenu.cpp').read_text(encoding='utf-8')
    sections = [
        ('void InGameQuickMenu::Close()', 'void InGameQuickMenu::SelectPrevious()'),
        ('void InGameQuickMenu::ResetWorld()', 'void InGameQuickMenu::Initialize()'),
        ('void InGameQuickMenu::Initialize()', 'void InGameQuickMenu::EnsureInputHook()'),
        ('LRESULT CALLBACK InGameQuickMenu::InputThunk(', 'void InGameQuickMenu::PostRenderThunk('),
        ('void InGameQuickMenu::PostRenderThunk(', 'UObject* InGameQuickMenu::FindLocalPlayerController('),
        ('void InGameQuickMenu::CaptureControllerInput(', 'void InGameQuickMenu::ReadCatalog()'),
    ]
    pieces = []
    for start, stop in sections:
        a = source.index(start)
        b = source.index(stop, a)
        pieces.append(source[a:b])
    lifecycle = '\n'.join(pieces)
    compiler = shutil.which(args.compiler)
    if not compiler:
        raise SystemExit(f'Compiler not found: {args.compiler}')
    with tempfile.TemporaryDirectory(prefix='runeschema-f2-test-') as name:
        temp = Path(name)
        (temp/'ProductionLifecycle.inc').write_text(lifecycle, encoding='utf-8')
        command = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic',
            '-I'+str(tests/'stubs'), '-I'+str(tests), '-I'+str(raw/'include'),
            '-I'+str(raw/'src/Generator'), '-I'+str(temp)]
        if args.ui_include:
            command += ['-I'+str(args.ui_include)]
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        command += [str(tests/'test_input.cpp'), '-o', str(temp/'test_input')]
        print('Compiler:', subprocess.check_output([compiler, '--version'], text=True).splitlines()[0], flush=True)
        print('Production lifecycle SHA256:', hashlib.sha256(lifecycle.encode()).hexdigest(), flush=True)
        subprocess.run(command, check=True)
        subprocess.run([str(temp/'test_input')], check=True)

if __name__ == '__main__':
    main()
