"""Check public loader walkthrough structure and JSONC mock-up syntax offline."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"

def uncomment(value):
    # Preserve strings, including URLs; remove JSONC comments outside strings.
    token = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/')
    value = token.sub(lambda m: m[0] if m[0].startswith('"') else '', value)
    return re.sub(r',\s*([}\]])', r'\1', value)

class Walkthroughs(unittest.TestCase):
    def test_all_loaders_have_workflow_mockup_and_tie_ins(self):
        pages = sorted((DOCS / "reference/loaders").glob("*.md"))
        self.assertEqual(len(pages), 22)
        for page in pages:
            text = page.read_text(encoding="utf-8")
            with self.subTest(page=page.name):
                self.assertIn("## Step-by-step workflow", text)
                self.assertIn("4. ", text)
                self.assertIn("**Tie-ins:**", text)
                self.assertIn("**mock-ups**", text)
                self.assertIn("```jsonc\n", text)
                self.assertNotRegex(text, r"RSv16|RuneSchema2VendorTest|GreatTree|installed r6")

    def test_jsonc_examples_parse(self):
        pages = list((DOCS / "reference/loaders").glob("*.md")) + [DOCS / "modders/quests-and-questboards.md", DOCS / "QUEST-SCROLLS.md"]
        for page in pages:
            for index, block in enumerate(re.findall(r"```jsonc\s*\n(.*?)\n```", page.read_text(encoding="utf-8"), re.S)):
                with self.subTest(page=page.name, block=index):
                    text = uncomment(block).strip()
                    # Documented field/choice fragments remain fragments, not files.
                    if not text.startswith(("{", "[")):
                        text = "{" + text + "}"
                    json.loads(text)

    def test_downloads_match_walkthrough(self):
        guide = (DOCS / "modders/quests-and-questboards.md").read_text(encoding="utf-8")
        blocks = [json.loads(uncomment(b)) for b in re.findall(r"```jsonc\s*\n(.*?)\n```", guide, re.S)]
        for file in (DOCS / "examples/QuestBoardWalkthrough").rglob("*.jsonc"):
            self.assertIn(json.loads(uncomment(file.read_text(encoding="utf-8"))), blocks)

    def test_loose_registry_starter_and_cross_links(self):
        page = DOCS / "reference/loaders/registry.md"
        text = page.read_text(encoding="utf-8")
        blocks = [json.loads(uncomment(b)) for b in re.findall(r"```jsonc\s*\n(.*?)\n```", text, re.S)]
        starter = DOCS / "examples/LooseRegistryWalkthrough/GoldenArsenal/registry/10-native-items.jsonc"
        document = json.loads(uncomment(starter.read_text(encoding="utf-8")))
        self.assertIn(document, blocks)
        self.assertEqual(document["SchemaVersion"], 1)
        self.assertEqual(document["Entries"], [])
        self.assertEqual(len(document["NativeRegistries"]["Items"]), 1)
        self.assertNotIn("RegistryOwner", document)
        self.assertIn("GoldenArsenal : 1", text)
        self.assertIn("[NATIVE-REGISTRY][DECLARED]", text)
        for name in ("LOADER-WALKTHROUGHS.md", "COOKED-PAK-REGISTRY-MANIFEST.md", "WEAPONS-AND-ITEMS.md"):
            content = (DOCS / name).read_text(encoding="utf-8")
            self.assertIn("registry.md#register-a-cooked-item-without-cooking-a-registry-declaration", content)
            self.assertNotIn("accepted only from a cooked registry asset", content)
            self.assertNotIn("file cannot\ndeclare `NativeRegistries`", content)

if __name__ == "__main__":
    unittest.main()
