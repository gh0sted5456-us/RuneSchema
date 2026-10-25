"""Catalogue completeness, emission audit and direct-anchor regressions."""
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


audit = load("diagnostic_audit", ROOT / "build/audit-diagnostics.py")
generate = load("diagnostic_generate", ROOT / "build/generate-diagnostics.py")


class Diagnostics(unittest.TestCase):
    def test_generated_files_and_shared_urls(self):
        generated = generate.outputs()
        for path, text in generated.items():
            self.assertEqual(path.read_text(encoding="utf-8"), text, str(path))
        document = json.loads(generated[ROOT / "docs/assets/data/diagnostic-codes.json"])
        urls = [row["url"] for row in document["codes"] + document["structuredCodes"] + document["bridgeReasons"]]
        self.assertEqual(len(urls), len(set(urls)))
        for url in urls:
            self.assertTrue(url.startswith(generate.REFERENCE + "#"))
        for row in document["codes"] + document["bridgeReasons"]:
            self.assertTrue(row["causes"] and row["troubleshooting"])
        self.assertEqual([row["id"] for row in document["codes"][:8]], [
            "QuestOwnerConflict", "RegistryDeferred", "RegistryMismatch", "WearableUnenrolled",
            "SlowWork", "CatalogUnavailable", "PlacementPending", "Unclassified"])

    def test_all_production_codes_are_documented(self):
        result = audit.audit()
        catalogue = json.loads((ROOT / "resources/diagnostics/codes.json").read_text(encoding="utf-8"))
        self.assertEqual(result["legacyCodes"], len(catalogue["codes"]))
        self.assertEqual(result["structuredCodes"], 222)
        self.assertEqual(result["registeredCodes"], len({row["code"] for row in catalogue["codes"]} | {row["code"] for row in generate.structured_catalogue()}))
        self.assertLessEqual(result["emittedCodes"], result["registeredCodes"])
        self.assertIn("callback_contract_mismatch", result["bridgeReasons"])

    def test_every_code_has_an_area_and_family_section(self):
        output = generate.outputs()
        document = json.loads(output[ROOT / "docs/assets/data/diagnostic-codes.json"])
        page = output[ROOT / "docs/ERROR-CODES.md"]
        for kind, rows in (("legacy", document["codes"]), ("structured", document["structuredCodes"])):
            for row in rows:
                area = row["subsystem"] if kind == "legacy" else row["category"]
                self.assertTrue(area)
                family = row["code"].rsplit("-", 1)[0].lower()
                self.assertIn("{#"+kind+"-family-"+family+"}", page)

    def fixture(self, root):
        for relative in ("resources/diagnostics/codes.json", "resources/diagnostics/bridge-reasons.json", "docs/ERROR-CODES.md", "docs/assets/data/diagnostic-codes.json", "source/raw/src/Runtime/RegistryBridge.cpp"):
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, path)

    def test_new_code_and_typed_id_cannot_escape_audit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            path = root / "source/raw/src/new.cpp"
            path.write_text('// [RS-FUTURE-999]\n/* Diagnostics::Id::Missing */\n', encoding="utf-8")
            audit.audit(root)  # Comment-only tokens are not emissions.
            path.write_text('throw std::runtime_error("[RS-FUTURE-999] new failure");', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Undocumented emitted code RS-FUTURE-999"):
                audit.audit(root)
            path.write_text('"[RS-FUTURE-1234] malformed code"', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Undocumented emitted code RS-FUTURE-1234"):
                audit.audit(root)
            path.write_text('CodedLog(PS::Diagnostics::Id::Missing, "condition");', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Unknown diagnostic ID Missing"):
                audit.audit(root)
            path.unlink()
            test = root / "source/helpy/source/tests/future.cpp"
            test.parent.mkdir(parents=True)
            test.write_text('"[RS-FUTURE-999]"', encoding="utf-8")
            audit.audit(root)  # Intentional future-code fixtures stay test-only.

    def test_new_bridge_reason_and_missing_header_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            bridge = root / "source/raw/src/Runtime/RegistryBridge.cpp"
            bridge.write_text('Trace("channel",nullptr,"entity","action",0,{},"stage","rejected","new_failure");', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Undocumented bridge reason: new_failure"):
                audit.audit(root)
            bridge.write_text('Trace("channel",nullptr,"entity","action",0,{},"stage","rejected",unreviewedReason);', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Unreviewed dynamic bridge reason expression"):
                audit.audit(root)
            bridge.write_text('', encoding="utf-8")
            page = root / "docs/ERROR-CODES.md"
            page.write_text(page.read_text(encoding="utf-8").replace("{#rs-cache-001}", ""), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Missing documentation anchor: rs-cache-001"):
                audit.audit(root)

    def test_actual_rendered_heading_ids_not_arbitrary_element_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            site = Path(directory)
            page = site / "ERROR-CODES/index.html"
            page.parent.mkdir()
            page.write_text('<h2 id="rs-cache-001">Cache</h2><h3 id="bridge-accepted">Accepted</h3>', encoding="utf-8")
            audit.rendered_anchors(site, {"rs-cache-001", "bridge-accepted"})
            page.write_text('<div id="rs-cache-001">Not a header</div>', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Rendered heading missing"):
                audit.rendered_anchors(site, {"rs-cache-001"})


    def test_shared_labels_keep_both_meanings_and_existing_links(self):
        generated = generate.outputs()
        data = json.loads(generated[ROOT / "docs/assets/data/diagnostic-codes.json"])
        shared = [row for row in data["structuredCodes"] if row["legacyCollision"]]
        self.assertEqual({row["code"] for row in shared}, {"RS-JRN-001", "RS-JRN-002", "RS-SPAWN-001"})
        page = generated[ROOT / "docs/ERROR-CODES.md"]
        for row in shared:
            self.assertIn("{#"+row["code"].lower()+"}", page)
            self.assertIn("{#structured-"+row["code"].lower()+"}", page)
        equipment = next(row for row in data["structuredCodes"] if row["code"]=="RS-EQP-006")
        self.assertIn("definition", equipment["troubleshooting"])

if __name__ == "__main__":
    unittest.main()
