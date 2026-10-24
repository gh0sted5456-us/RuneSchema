"""Offline, fail-closed diagnostic catalogue/emission/link audit (stdlib only)."""
import argparse
from html.parser import HTMLParser
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CODE = re.compile(r"\bRS-[A-Z][A-Z0-9]*-\d+\b")
# Preserve strings and line numbers; commented examples must not count as emitters.
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/')


def uncomment(text):
    return TOKENS.sub(lambda m: re.sub(r"[^\n]", " ", m[0]) if m[0].startswith(("//", "/*")) else m[0], text)


def arguments(text, start):
    """Split a C++ call outside strings/nested parentheses, braces and brackets."""
    parts, begin, stack, pos = [], start, [], start
    while pos < len(text):
        char = text[pos]
        if char in "\"'":
            quote = char
            pos += 1
            while pos < len(text):
                if text[pos] == "\\":
                    pos += 2
                    continue
                if text[pos] == quote:
                    break
                pos += 1
        elif char in "([{":
            stack.append(char)
        elif char in ")]}":
            if not stack:
                parts.append(text[begin:pos])
                return parts
            stack.pop()
        elif char == "," and not stack:
            parts.append(text[begin:pos])
            begin = pos + 1
        pos += 1
    raise ValueError("Unclosed diagnostic call")


def source_files(root):
    scopes = ("source/raw/src", "source/raw/include", "source/raw/plugins", "source/raw/tools",
              "source/helpy/source", "source/plugins", "source/tools", "source/settings", "plugins", "build")
    suffixes = {".cpp", ".h", ".hpp", ".inl", ".c", ".lua", ".py", ".ps1", ".json", ".jsonc"}
    for scope in scopes:
        folder = root / scope
        if not folder.exists():
            continue
        for path in sorted(folder.rglob("*")):
            if path.suffix.lower() not in suffixes or any(part in {"tests", "vendor", ".cache", "__pycache__"} for part in path.relative_to(folder).parts):
                continue
            if path.name in {"DiagnosticCodes.h", "BridgeReasonCodes.h", "StructuredDiagnosticRegistry.h", "StructuredDiagnosticAdvice.h", "generate-diagnostics.py", "audit-diagnostics.py"}:
                continue
            yield path


def audit(root=ROOT):
    catalogue = json.loads((root / "resources/diagnostics/codes.json").read_text(encoding="utf-8"))["codes"]
    codes = {row["code"] for row in catalogue}
    public = json.loads((root / "docs/assets/data/diagnostic-codes.json").read_text(encoding="utf-8"))
    structured = public.get("structuredCodes", [])
    codes |= {row["code"] for row in structured}
    ids = {row["id"]: row["code"] for row in catalogue}
    reasons = json.loads((root / "resources/diagnostics/bridge-reasons.json").read_text(encoding="utf-8"))
    sites, errors, scanned = {}, [], 0
    for path in source_files(root):
        scanned += 1
        text = uncomment(path.read_text(encoding="utf-8-sig"))
        relative = path.relative_to(root).as_posix()
        for match in CODE.finditer(text):
            code = match[0]
            site = f"{relative}:{text.count(chr(10), 0, match.start()) + 1}"
            if code not in codes:
                errors.append(f"Undocumented emitted code {code}: {site}")
            sites.setdefault(code, set()).add(site)
        for match in re.finditer(r"\bDiagnostics::Id::([A-Za-z0-9_]+)", text):
            site = f"{relative}:{text.count(chr(10), 0, match.start()) + 1}"
            if match[1] not in ids:
                errors.append(f"Unknown diagnostic ID {match[1]}: {site}")
            else:
                sites.setdefault(ids[match[1]], set()).add(site)
    bridge = uncomment((root / "source/raw/src/Runtime/RegistryBridge.cpp").read_text(encoding="utf-8-sig"))
    observed_reasons = set()
    for match in re.finditer(r"\bTrace\s*\(", bridge):
        args = arguments(bridge, match.end())
        if len(args) < 9:
            errors.append("Unrecognized bridge Trace signature; update the audit")
            continue
        literal_reasons = re.findall(r'"([a-z]+(?:_[a-z]+)*)"', args[8])
        if not literal_reasons and args[8].strip() not in {"reason", "const std::string& reason"}:
            errors.append(f"Unreviewed dynamic bridge reason expression: {args[8].strip()}")
        observed_reasons.update(literal_reasons)
    observed_reasons.update(re.findall(r'\breason\s*=\s*"([a-z_]+)"', bridge))
    for reason in sorted(observed_reasons - reasons.keys()):
        errors.append(f"Undocumented bridge reason: {reason}")
    page = (root / "docs/ERROR-CODES.md").read_text(encoding="utf-8")
    anchors = re.findall(r"\{#([^}]+)\}", page)
    expected = {code.lower() for code in codes} | {"bridge-" + code.replace("_", "-") for code in reasons}
    expected |= {row["anchor"] for row in structured}
    expected |= {"legacy-family-"+row["code"].rsplit("-",1)[0].lower() for row in catalogue}
    expected |= {"structured-family-"+row["code"].rsplit("-",1)[0].lower() for row in structured}
    if len(anchors) != len(set(anchors)):
        errors.append("Duplicate documentation anchors")
    for anchor in sorted(expected - set(anchors)):
        errors.append(f"Missing documentation anchor: {anchor}")
    if errors:
        raise ValueError("\n".join(errors))
    return dict(scannedFiles=scanned, registeredCodes=len(codes), legacyCodes=len(catalogue), structuredCodes=len(structured), emittedCodes=len(sites),
                catalogueOnlyCodes=sorted(codes - sites.keys()), bridgeReasons=sorted(observed_reasons),
                sites={code: sorted(rows) for code, rows in sorted(sites.items())}, anchors=sorted(expected))


def rendered_anchors(site, expected):
    class Headings(HTMLParser):
        def __init__(self):
            super().__init__()
            self.ids = []
        def handle_starttag(self, tag, attrs):
            if tag in {"h2", "h3"}:
                self.ids.extend(value for key, value in attrs if key == "id")
    parser = Headings()
    parser.feed((Path(site) / "ERROR-CODES/index.html").read_text(encoding="utf-8"))
    for anchor in expected:
        if parser.ids.count(anchor) != 1:
            raise ValueError(f"Rendered heading missing or duplicated: {anchor}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--site", type=Path, help="Validate actual MkDocs output heading IDs")
    parser.add_argument("--report", type=Path, help="Write an optional local source-site inventory")
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("generate_diagnostics", ROOT / "build/generate-diagnostics.py")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    for path, text in generator.outputs().items():
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            raise ValueError(f"Stale generated diagnostics: {path.relative_to(ROOT)}")
    result = audit()
    if args.site:
        rendered_anchors(args.site, result["anchors"])
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Audited {result['scannedFiles']} first-party files: {result['registeredCodes']} RS codes, "
          f"{result['emittedCodes']} with literal/typed source sites, {len(result['bridgeReasons'])} bridge reasons; all documented and anchored.")


if __name__ == "__main__":
    main()
