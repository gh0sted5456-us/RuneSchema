#!/usr/bin/env python3
"""Lightweight delimiter check for changed C++ files; not a compiler."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
files = [
    "raw/include/Generator/LoaderSchemas.h",
    "raw/include/Generator/QuickMenuUI.h",
    "raw/include/Generator/InspectionTools.h",
    "raw/src/Generator/InspectionTools.cpp",
    "raw/src/dllmain.cpp",
]
pairs = {"}": "{", "]": "[", ")": "("}
opens = set(pairs.values())


def check(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    stack: list[tuple[str, int]] = []
    quote = None
    escaped = line_comment = block_comment = False
    index = 0
    while index < len(text):
        char = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and following == "/":
                block_comment = False
                index += 1
        elif quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and following == "/":
            line_comment = True
            index += 1
        elif char == "/" and following == "*":
            block_comment = True
            index += 1
        elif char in ('"', "'"):
            quote = char
        elif char in opens:
            stack.append((char, index))
        elif char in pairs:
            if not stack or stack[-1][0] != pairs[char]:
                raise AssertionError(f"{path}: unmatched {char} at byte {index}")
            stack.pop()
        index += 1
    if quote or block_comment or stack:
        raise AssertionError(f"{path}: unterminated token; stack tail={stack[-5:]}")
    print(f"PASS {path.relative_to(root)}")


for relative in files:
    check(root / relative)
print("PASS changed C++ delimiters and literals are balanced (lexical check only)")
