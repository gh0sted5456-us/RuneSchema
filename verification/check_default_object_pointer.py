"""Compile the exact default-object guard with a small TObjectPtr stand-in.

This tests C++ pointer deduction, pointer identity, null handling and lifecycle
flag guards. It is NOT a compilation against UE4SS, MSVC, or the game runtime.
The negative control restores the original expression and must fail to compile.
Uses Python's standard library and an installed C++23 compiler only.

Run: python check_default_object_pointer.py --compiler g++
     python check_default_object_pointer.py --compiler clang++ --sanitize
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, timeout=60)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if compiler is None:
        raise RuntimeError(f"C++ compiler not found: {args.compiler}")

    root = Path(__file__).resolve().parents[1]
    catalogue = root / "raw/src/Loader/QuickMenuCatalog.inl"
    guards = [line.strip() for line in catalogue.read_text(encoding="utf-8").splitlines()
              if "if(auto* defaults=" in line]
    if len(guards) != 1:
        raise RuntimeError("Expected exactly one default-object guard in the catalogue.")
    guard = guards[0]
    if "type->GetClassDefaultObject().Get();" not in guard:
        raise RuntimeError("The explicit TObjectPtr.Get() fix is missing.")
    gui = (root / "raw/src/Generator/InGameQuickMenu.cpp").read_text(encoding="utf-8")
    if "Unreal/Property/FNumericProperty.hpp" in gui:
        raise RuntimeError("The old FNumericProperty forwarding include is still present.")
    if "#include <Unreal/CoreUObject/UObject/UnrealType.hpp>" not in gui:
        raise RuntimeError("The replacement UnrealType include is missing.")

    prefix = r'''
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>

enum EObjectFlags : unsigned {
    RF_BeginDestroyed=1, RF_FinishDestroyed=2, RF_NeedLoad=4, RF_NeedPostLoad=8
};
struct UObject {
    unsigned flags=0;
    std::int32_t metadata=123;
    bool HasAnyFlags(EObjectFlags wanted) const {
        return (flags & static_cast<unsigned>(wanted)) != 0;
    }
};
// Stand-in contract: wrapper returns a raw pointer through Get(). Even providing
// an implicit conversion does not make auto* deduction from the wrapper valid.
template<class T> struct TObjectPtr {
    T* object;
    T* Get() const { return object; }
    operator T*() const { return object; }
};
struct UClass {
    UObject* object;
    int reads=0;
    TObjectPtr<UObject> GetClassDefaultObject() { ++reads; return {object}; }
};
struct NumericProperty {
    template<class T> T* ContainerPtrToValuePtr(void* container) const {
        return static_cast<T*>(&static_cast<UObject*>(container)->metadata);
    }
};
UObject* ResolveDefaultForMetadata(UClass* type) {
'''
    suffix = r'''
        static_assert(std::is_same_v<decltype(defaults), UObject*>);
        NumericProperty property;
        auto* ptr=property.ContainerPtrToValuePtr<void>(defaults);
        if(ptr != static_cast<void*>(&defaults->metadata))
            throw std::runtime_error("Metadata pointer did not use the same object.");
        return defaults;
    }
    return nullptr;
}
int main() {
    UClass empty{nullptr};
    if(ResolveDefaultForMetadata(&empty) || empty.reads != 1) return 1;
    const unsigned cases[]={0, RF_BeginDestroyed, RF_FinishDestroyed,
        RF_NeedLoad, RF_NeedPostLoad, 15};
    for(unsigned flags:cases) {
        UObject object{flags,123};
        UClass type{&object};
        auto* result=ResolveDefaultForMetadata(&type);
        if(result != (flags==0 ? &object : nullptr)) return 2;
        if(type.reads!=1 || object.flags!=flags || object.metadata!=123) return 3;
    }
    std::cout << "PASS: 7 guard cases; raw pointer type, identity, null, lifecycle flags, "
                 "single retrieval and unchanged metadata.\n";
}
'''
    source = prefix + guard + "\n" + suffix
    negative = source.replace("GetClassDefaultObject().Get();", "GetClassDefaultObject();", 1)
    flags = ["-std=c++23", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
    with tempfile.TemporaryDirectory(prefix="rs7-pointer-regression-") as temp:
        directory = Path(temp)
        bad_file = directory / "negative.cpp"
        good_file = directory / "fixed.cpp"
        bad_file.write_text(negative, encoding="utf-8")
        good_file.write_text(source, encoding="utf-8")
        bad = run([compiler, *flags, "-fsyntax-only", str(bad_file)])
        if bad.returncode == 0 or "auto" not in bad.stderr:
            raise RuntimeError("The original expression did not reproduce the deduction failure.\n" + bad.stderr)
        print("PASS: original auto* = TObjectPtr expression rejected (negative control).")
        if args.sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        exe = directory / "fixed"
        good = run([compiler, *flags, str(good_file), "-o", str(exe)])
        if good.returncode:
            raise RuntimeError("Patched guard did not compile.\n" + good.stderr)
        result = run([str(exe)])
        if result.returncode or result.stderr:
            raise RuntimeError("Patched guard regression failed.\n" + result.stdout + result.stderr)
        print(result.stdout, end="")
        print("PASS: obsolete numeric-property include removed; UnrealType include retained.")
    print("Scope: C++ stand-ins, not real UE4SS/MSVC or in-game validation.")


if __name__ == "__main__":
    main()
