# Native-independent patch tests

Compile field-patches.cpp together with src/Core/JsonArrayPatch.cpp,
src/Core/JsonPatchDirective.cpp and src/Core/JsonLoadOrderMerge.cpp using
C++23 and include/ plus the nlohmann/json 3.11.3 include directory.
Run the resulting executable; it should report 20 checks passed.

These cover directive validation, index bounds, unique/missing/ambiguous
selection, protected identities and the existing recursive object merge.
They do not simulate Unreal reflection, material creation or ImGui rendering.
