"""Check channel isolation, scoped navigation and the public documentation build."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("prepare_docs", ROOT / "build/prepare-docs.py")
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class Channels(unittest.TestCase):
    def fixture(self, root):
        stable, experimental = root / "stable", root / "experimental"
        for folder in (stable / "docs/experimental", experimental / "docs/experimental", experimental / "docs/assets/data", experimental / "build"):
            folder.mkdir(parents=True, exist_ok=True)
        (stable / "mkdocs.yml").write_text('site_name: Stable\nnav:\n  - Home: index.md\n  - Experimental:\n      - Old: experimental/old.md\nextra:\n  plugin: !!python/name:builtins.str\n', encoding="utf-8")
        (stable / "docs/index.md").write_text("Stable instructions\n", encoding="utf-8")
        (stable / "docs/experimental/old.md").write_text("Stale experimental page", encoding="utf-8")
        (experimental / "docs/index.md").write_text("Must not replace main", encoding="utf-8")
        (experimental / "docs/experimental/index.md").write_text("Experimental feature\n", encoding="utf-8")
        (experimental / "docs/ERROR-CODES.md").write_text("Shared code anchors", encoding="utf-8")
        (experimental / "docs/assets/data/diagnostic-codes.json").write_text("{}", encoding="utf-8")
        (experimental / "build/docs-experimental-nav.json").write_text('[{"Overview":"experimental/index.md"}]', encoding="utf-8")
        return stable, experimental

    def test_stable_content_and_configuration_are_preserved(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".cache") as folder:
            root = Path(folder); stable, experimental = self.fixture(root); output = root / "site-source"
            metadata = prepare.prepare(stable, experimental, output, "stable-sha", "experimental-sha")
            self.assertEqual((output / "docs/index.md").read_bytes(), (stable / "docs/index.md").read_bytes())
            self.assertEqual((output / "docs/experimental/index.md").read_text(), "Experimental feature\n")
            self.assertFalse((output / "docs/experimental/old.md").exists())
            config = (output / "mkdocs.yml").read_text()
            self.assertIn("!!python/name:builtins.str", config)
            self.assertEqual(config.count("Experimental:"), 1)
            self.assertIn("Home: index.md", config)
            self.assertEqual((output / "docs/ERROR-CODES.md").read_text(), "Shared code anchors")
            self.assertEqual(metadata["stable"]["commit"], "stable-sha")
            with self.assertRaisesRegex(ValueError, "already exists"):
                prepare.prepare(stable, experimental, output)

    def test_navigation_cannot_replace_main_pages(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".cache") as folder:
            stable, experimental = self.fixture(Path(folder))
            for path in ("index.md", "experimental/../../index.md", "experimental/missing.md", "experimental\\index.md"):
                with self.assertRaises(ValueError):
                    prepare.navigation([{"Feature": path}], experimental / "docs")

    def test_repository_guides_have_promotion_destinations(self):
        entries = json.loads((ROOT / "build/docs-experimental-nav.json").read_text())
        prepare.navigation(entries, ROOT / "docs")
        for path in (ROOT / "docs/experimental").rglob("*.md"):
            text = path.read_text(encoding="utf-8")
            front = yaml.safe_load(text.split("---", 2)[1])
            self.assertEqual(front["channel"], "experimental", str(path))
            if path.name != "index.md":
                destination = front["promote_to"]
                self.assertNotIn("..", Path(destination).parts)
                self.assertFalse(Path(destination).is_absolute())

    def test_deployment_uses_both_sources_and_one_concurrency_group(self):
        workflow = yaml.load((ROOT / ".github/workflows/docs-pages.yml").read_text(), Loader=yaml.BaseLoader)
        self.assertEqual(workflow["on"]["push"]["branches"], ["main", "experimental"])
        self.assertEqual(workflow["jobs"]["deploy"]["concurrency"]["group"], "pages")
        self.assertEqual(workflow["jobs"]["deploy"]["if"], "github.ref == 'refs/heads/main'")
        dispatch = next(step for step in workflow["jobs"]["build"]["steps"] if step["name"] == "Request public-site rebuild from main")
        self.assertEqual(dispatch["if"], "github.ref == 'refs/heads/experimental'")
        self.assertIn('--data \'{"ref":"main"}\'', dispatch["run"])
        self.assertEqual(workflow["permissions"]["actions"], "write")
        checkouts = [step["with"] for step in workflow["jobs"]["build"]["steps"] if step.get("uses", "").startswith("actions/checkout@")]
        self.assertEqual([row["ref"] for row in checkouts], ["main", "experimental"])


if __name__ == "__main__":
    (ROOT / ".cache").mkdir(exist_ok=True)
    unittest.main()
