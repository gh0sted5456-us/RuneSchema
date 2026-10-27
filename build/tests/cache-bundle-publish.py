"""Offline contract checks for vanilla bundle publication staging."""
import importlib.util
import hashlib
import json
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("publisher", Path(__file__).resolve().parents[1]/"publish-cache-bundle.py")
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)

class Publication(unittest.TestCase):
    def map_data(self, change=lambda doc: None):
        doc = json.loads(self.data()[12:])
        pixels = b"RSI3"+struct.pack("<III", 2, 1, 8)+bytes(range(8))
        doc["WorldMap"] = {"Version": 1, "Asset": "/Game/Art/UI/Map/T_WorldMap_Landmass.T_WorldMap_Landmass",
                           "Bytes": len(pixels), "SHA256": hashlib.sha256(pixels).hexdigest(),
                           "Projection": {"Center": [100, 200], "Right": [1, 0], "Up": [0, -1],
                                          "HalfWidth": 1000, "HalfHeight": 500, "MinZ": -100, "MaxZ": 100}}
        change(doc)
        text = json.dumps(doc).encode()
        return b"RSCACHE1"+struct.pack("<I", len(text))+text+pixels

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

    def test_mod_scope_cannot_publish_as_native(self):
        doc = json.loads(self.data()[12:])
        doc["ContentOwner"] = "mods/example"
        text = json.dumps(doc).encode()
        with self.assertRaises(ValueError):
            publisher.inspect(b"RSCACHE1"+struct.pack("<I",len(text))+text)

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

    def test_native_plus_object_name(self):
        doc = json.loads(self.data()[12:])
        doc["Records"][0]["Path"] = "/Game/UI/T_Skill_Recall+.T_Skill_Recall+"
        text = json.dumps(doc).encode()
        self.assertEqual(publisher.inspect(b"RSCACHE1"+struct.pack("<I",len(text))+text)["Records"][0]["Path"], doc["Records"][0]["Path"])

    def test_map_with_items_and_map_only(self):
        self.assertEqual(publisher.inspect(self.map_data())["WorldMap"]["Bytes"], 24)
        doc = publisher.inspect(self.map_data(lambda doc: doc.update(Records=[])))
        self.assertEqual(doc["Records"], [])

    def test_map_corruption_and_trailing_bytes(self):
        data = self.map_data()
        for invalid in (data[:-1], data[:-1]+b"X", data+b"unexpected"):
            with self.subTest(size=len(invalid)), self.assertRaises(ValueError):
                publisher.inspect(invalid)

    def test_invalid_map_metadata(self):
        for key, value in (("Bytes", True), ("Bytes", 16+2048*2048*4+1), ("SHA256", "0"*64),
                           ("Asset", "/Game/Other.Other"), ("Version", True), ("Players", ["private"])):
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                publisher.inspect(self.map_data(lambda doc: doc["WorldMap"].update({key: value})))

    def test_invalid_map_alignment(self):
        for key, value in (("Center", [0]), ("Center", [float("nan"), 0]), ("Right", [1, 1]),
                           ("Up", [1, 0]), ("HalfWidth", 0), ("HalfHeight", True), ("MinZ", 100),
                           ("MaxZ", float("inf")), ("World", "private")):
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                publisher.inspect(self.map_data(lambda doc: doc["WorldMap"]["Projection"].update({key: value})))

    def test_map_dimensions_and_mod_scope(self):
        data = bytearray(self.map_data())
        length, = struct.unpack_from("<I", data, 8)
        blob = b"RSI3"+struct.pack("<III", 0, 2, 8)+bytes(range(8))
        doc = json.loads(data[12:12+length])
        doc["WorldMap"]["SHA256"] = hashlib.sha256(blob).hexdigest()
        text = json.dumps(doc).encode()
        with self.assertRaises(ValueError):
            publisher.inspect(b"RSCACHE1"+struct.pack("<I", len(text))+text+blob)
        with self.assertRaises(ValueError):
            publisher.inspect(self.map_data(lambda doc: doc.update(ContentOwner="mods/example")))

if __name__ == "__main__":
    unittest.main()

