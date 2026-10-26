"""Check the download contains the reviewed launcher and usable standalone help."""
import hashlib
import io
import json
import unittest
import zipfile
import package


class AuthoringPackage(unittest.TestCase):
    def test_archive_and_manifest(self):
        outputs = package.outputs()
        self.assertEqual(outputs, package.outputs())
        index = json.loads(next(data for path, data in outputs.items() if path.name == "index.json"))
        archive = next(data for path, data in outputs.items() if path.suffix == ".zip")
        self.assertEqual(hashlib.sha256(archive).hexdigest(), index["SHA256"])
        self.assertEqual(len(archive), index["Bytes"])
        self.assertEqual(index["File"], index["SHA256"] + ".zip")
        with zipfile.ZipFile(io.BytesIO(archive)) as z:
            self.assertIsNone(z.testzip())
            self.assertEqual(set(z.namelist()), {"README.md", "build-helpy-cache.sh", "build-helpy-cache.ps1", "Build-Helpy-Cache.bat", "LICENSE", "DEPENDENCIES.md"})
            self.assertEqual(z.read("build-helpy-cache.sh").decode(), package.render())
            self.assertEqual(z.read("build-helpy-cache.ps1").decode("utf-8-sig"), package.render_powershell())
            self.assertEqual(z.read("Build-Helpy-Cache.bat").decode(), package.DRAG_DROP)
            readme = z.read("README.md").decode()
            for required in ["Git Bash", ".NET 10", "Content/Paks", "--mapping", "--oodle", "--mod-root", "--base-game", "--include-mods", "offline-helpy.rsmk-cache"]:
                self.assertIn(required, readme)
            self.assertIn("MIT License", z.read("LICENSE").decode())
            self.assertIn("CUE4Parse", z.read("DEPENDENCIES.md").decode())


if __name__ == "__main__":
    unittest.main()
