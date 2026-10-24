"""Offline contract checks for vanilla bundle publication staging."""
import importlib.util
import json
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("publisher", Path(__file__).resolve().parents[1]/"publish-cache-bundle.py")
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)

class Publication(unittest.TestCase):
    def data(self, owner="core"):
        doc = {"Version": 2, "Kind": "RuneSchemaCacheBundle", "Locale": "en-US",
               "Sources": {"game": "a"*64}, "Icons": [],
               "Records": [{"Kind": "items", "Owner": owner, "Source": "game",
                            "Path": "/Game/Items/DA_Example.DA_Example", "PersistenceID": "",
                            "Data": {"Name": "Illustrative fixture only", "ObservedCurrent": False}}]}
        text = json.dumps(doc).encode()
        return b"RSCACHE1" + struct.pack("<I", len(text)) + text

    def test_metadata_only(self):
        doc = publisher.inspect(self.data())
        self.assertEqual(doc["Records"][0]["PersistenceID"], "")

    def test_mod_cannot_publish_as_vanilla(self):
        with self.assertRaises(ValueError):
            publisher.inspect(self.data("mods/example"))

    def test_trailing_payload(self):
        with self.assertRaises(ValueError):
            publisher.inspect(self.data()+b"unexpected")

    def test_private_metadata(self):
        data = self.data()
        doc = json.loads(data[12:])
        doc["Records"][0]["Data"]["RuntimeHandle"] = 1234
        text = json.dumps(doc).encode()
        with self.assertRaises(ValueError):
            publisher.inspect(b"RSCACHE1"+struct.pack("<I",len(text))+text)

    def test_native_hyphenated_object_name(self):
        doc = json.loads(self.data()[12:])
        doc["Records"][0]["Path"] = "/Game/Food/ITEM_Stir-Fry.ITEM_Stir-Fry"
        text = json.dumps(doc).encode()
        self.assertEqual(publisher.inspect(b"RSCACHE1"+struct.pack("<I",len(text))+text)["Records"][0]["Path"], doc["Records"][0]["Path"])

if __name__ == "__main__":
    unittest.main()

