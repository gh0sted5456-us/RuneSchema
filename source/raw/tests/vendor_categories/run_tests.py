#!/usr/bin/env python3
"""Compile the actual native-label bridge against explicit test doubles.

Requires Python 3.9+ and g++ or clang++. No Unreal/JSON dependency is downloaded.
Use --compiler clang++ or --sanitize for an address/undefined sanitizer run.
This is NOT a game test or a production DLL build.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        raise SystemExit(f'C++ compiler not found: {args.compiler}')
    tests = Path(__file__).resolve().parent
    raw = tests.parents[1]
    source = raw / 'src/Loader/VendorCategoryNativeText.cpp'
    headers = [
        'Unreal/CoreUObject/UObject/Class.hpp',
        'Unreal/CoreUObject/UObject/UnrealType.hpp',
        'Unreal/CoreUObject/UObject/FStrProperty.hpp',
        'Unreal/Property/FTextProperty.hpp', 'Unreal/FString.hpp',
        'Unreal/NameTypes.hpp', 'Unreal/UFunctionStructs.hpp', 'Unreal/UObject.hpp',
        'SDK/Classes/Custom/UObjectGlobals.h',
    ]
    with tempfile.TemporaryDirectory(prefix='runeschema-category-test-') as directory:
        build = Path(directory)
        for name in headers:
            path = build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#pragma once\n#include "FakeUnreal.h"\n', encoding='utf-8')
        binary = build / ('category_tests.exe' if os.name == 'nt' else 'category_tests')
        command = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-g',
                   '-I', str(build), '-I', str(tests), '-I', str(raw/'include'),
                   str(source), str(tests/'test_categories.cpp'), '-o', str(binary)]
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
    # These are source-wiring checks, not execution of the JSON/group loaders.
    offers = (raw/'include/Loader/VendorOffers.h').read_text(encoding='utf-8')
    npc = (raw/'src/Loader/DragonWildsNpcLoader.cpp').read_text(encoding='utf-8')
    recipes = (raw/'src/Loader/DragonWildsRecipeModLoader.cpp').read_text(encoding='utf-8')
    groups = (raw/'src/Loader/VendorCategoryText.cpp').read_text(encoding='utf-8')
    group_header = (raw/'include/Loader/VendorCategoryGroups.h').read_text(encoding='utf-8')
    method = npc.split('void DragonWildsNpcLoader::CreateInlineMerchantRow(', 1)[1].split(
        'bool DragonWildsNpcLoader::HasLiveTableLease(', 1)[0]
    update = method.split('if(existingOwned) {', 1)[1].split('return;', 1)[0]
    creation = method.split('dataTable->AddRow(rowName,', 1)[1]
    native_creation = creation.split('if (nativeCategories)', 1)[1].split('else if (wroteName)', 1)[0]
    custom_creation = creation.split('else if (wroteName)', 1)[1]
    checks = [
        ('literal-label validation remains wired',
         'VendorCategoryLabel::Validate(item.value("Category", std::string("Items")))' in offers),
        ('native group write remains wired',
         'VendorCategoryText::WriteGroups(rowData.GetData(), itemsArray, rowItems)' in npc),
        ('owned-row destination verification remains wired',
         'VendorCategoryText::VerifyGroups(existingOwned, itemsArray, rowItems)' in update),
        ('new-row destination verification remains wired',
         'VendorCategoryText::VerifyGroups(dataTable->FindRowUnchecked(rowName)' in native_creation),
        ('shared recipe label writing remains wired',
         'VendorCategoryText::Write(value.GetData(), labelProp' in recipes),
        ('shared recipe label reading remains wired',
         'VendorCategoryText::Read(categoryData, labelProp)' in recipes),
        ('exact readback validation remains active',
         'VendorCategoryLabel::RequireExact(wanted.at("Label").get<std::string>(), actual)' in groups),
        ('group verification has no success logger',
         'PS::Log' not in groups and 'Utility/Logging.h' not in groups),
        ('old per-category success message is absent',
         'Vendor category verified:' not in groups + npc),
        ('logging-specific verification parameters removed',
         'logSuccess' not in groups + group_header and 'std::string& owner' not in group_header),
        ('staged array verification remains unconditional',
         'VerifyGroups(row, groups, expected);' in groups),
        ('update produces one verbose summary with a category total',
         update.count('PS::Log<LogLevel::Verbose>') == 1
         and update.count('{} categories updated.') == 1
         and 'sourceItems.size(), rowItems.size());' in update),
        ('update success summary follows destination verification',
         update.index('VendorCategoryText::VerifyGroups') < update.index('PS::Log<LogLevel::Verbose>')),
        ('both create-name branches use verbose category totals',
         native_creation.count('PS::Log<LogLevel::Verbose>') == 2
         and native_creation.count('{} categories created.') == 2
         and native_creation.count('rowItems.size());') == 2),
        ('create success summaries follow destination verification',
         native_creation.index('VendorCategoryText::VerifyGroups') < native_creation.index('PS::Log<LogLevel::Verbose>')),
        ('native summaries do not emit at normal level',
         'PS::Log<LogLevel::Normal>' not in native_creation + update),
        ('custom ItemsProperty creation logs retain their prior behavior',
         custom_creation.count('PS::Log<LogLevel::Normal>') == 2
         and 'categories created' not in custom_creation),
        ('custom ItemsProperty updates retain their writer',
         'PropertyHelper::CopyJsonValueToContainer(existingOwned,itemsProperty,rowItems);' in update),
        ('group count is checked before summaries',
         'array->Num() != static_cast<int>(expected.size())' in groups),
        ('per-category recipe count validation is retained',
         'collection->Num() != static_cast<int>(wanted.at("Collection").size())' in groups),
    ]
    for name, passed in checks:
        if not passed:
            raise AssertionError(name)
    print(f'PASS: {len(checks)} source-wiring/logging checks (not runtime JSON/store tests).')


if __name__ == '__main__':
    main()
