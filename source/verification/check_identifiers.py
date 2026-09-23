"""Run after F2ModelTests from this directory. Standard library only."""
import base64
import re
from pathlib import Path

ids = Path("ids.txt").read_text(encoding="utf-8").splitlines()
assert len(ids) == 10_000 and len(set(ids)) == len(ids)
for value in ids:
    assert re.fullmatch(r"RS7Expand[0-9]{12}[AQgw]", value), value
    decoded = base64.urlsafe_b64decode(value + "==")
    assert len(decoded) == 16
    assert base64.urlsafe_b64encode(decoded).decode("ascii").rstrip("=") == value
print("PASS: 10,000 unique, canonical 22-character identifiers round-tripped to 128 bits.")
