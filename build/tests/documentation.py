"""Check rendered local links/anchors and tutorial snippet inputs without network access."""
import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
SITE_HOST = "gh0sted5456-us.github.io"
SITE_PREFIX = "/RuneSchema/"


class Page(HTMLParser):
    def __init__(self, content):
        super().__init__(convert_charrefs=True)
        self.anchors = set()
        self.links = []
        self.feed(content)

    def handle_starttag(self, tag, attrs):
        fields = dict(attrs)
        if fields.get("id"):
            self.anchors.add(fields["id"])
        if tag == "a" and fields.get("name"):
            self.anchors.add(fields["name"])
        for key in ("href", "src"):
            if fields.get(key):
                self.links.append(fields[key])


def audit(site):
    site = site.resolve()
    pages = {path: Page(path.read_text(encoding="utf-8")) for path in site.rglob("*.html")}
    errors = set()
    for source, page in pages.items():
        for link in page.links:
            url = urlsplit(link)
            if url.scheme or url.netloc:
                if url.scheme not in ("http", "https") or url.netloc != SITE_HOST:
                    continue
                if not url.path.startswith(SITE_PREFIX):
                    continue
            path = unquote(url.path)
            if path.startswith(SITE_PREFIX):
                path = path[len(SITE_PREFIX):]
                target = site / path
            elif path.startswith("/"):
                target = site / path.lstrip("/")
            elif path:
                target = source.parent / path
            else:
                target = source
            target = target.resolve()
            label = f"{source.relative_to(site)}: {link}"
            if not target.is_relative_to(site):
                errors.add(label + " (outside site)")
                continue
            if target.is_dir():
                target /= "index.html"
            if not target.is_file():
                errors.add(label + " (missing file)")
            elif url.fragment and target in pages and unquote(url.fragment) not in pages[target].anchors:
                errors.add(label + " (missing anchor)")
    return len(pages), sorted(errors)


def snippets():
    errors = []
    count = 0
    for source in (ROOT / "docs").rglob("*.md"):
        for relative in re.findall(r'^\s*--8<--\s+"([^"]+)"\s*$', source.read_text(encoding="utf-8"), re.M):
            count += 1
            target = (ROOT / relative).resolve()
            if not target.is_relative_to(ROOT) or not target.is_file():
                errors.append(f"{source.relative_to(ROOT)}: missing/unsafe snippet {relative}")
    return count, errors


class LinkTests(unittest.TestCase):
    def test_files_anchors_and_external_links(self):
        (ROOT / ".cache").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / ".cache") as folder:
            site = Path(folder)
            (site / "guide").mkdir()
            (site / "asset.jsonc").write_text("{}", encoding="utf-8")
            (site / "index.html").write_text('<a href="guide/#field">ok</a><a href="https://example.com/absent">external</a>', encoding="utf-8")
            page = site / "guide/index.html"
            page.write_text('<h1 id="field">Field</h1><a href="../asset.jsonc">file</a><a href="/RuneSchema/#top">root</a>', encoding="utf-8")
            count, errors = audit(site)
            self.assertEqual(count, 2)
            self.assertEqual(len(errors), 1)
            self.assertIn("missing anchor", errors[0])
            (site / "index.html").write_text('<h1 id="top">Top</h1><a href="guide/#field">ok</a>', encoding="utf-8")
            self.assertEqual(audit(site)[1], [])
            page.write_text('<h1 id="field">Field</h1><a href="../missing">missing</a><a href="../../escape">escape</a><a href="https://gh0sted5456-us.github.io/RuneSchema/nope/">own site</a>', encoding="utf-8")
            self.assertEqual(len(audit(site)[1]), 3)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(LinkTests))
        if not result.wasSuccessful():
            raise SystemExit(1)
    count, errors = snippets()
    print(f"Checked {count} documentation snippet inputs.")
    if args.site:
        if not (args.site / "index.html").is_file():
            parser.error("--site must contain the built index.html")
        pages, failures = audit(args.site)
        errors.extend(failures)
        print(f"Checked local links and anchors across {pages} rendered pages.")
    for error in errors:
        print(error)
    raise SystemExit(bool(errors))
