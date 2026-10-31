"""Assemble main's guides and experimental's feature area without mixing runtime channels."""
import argparse
import json
from pathlib import Path, PurePosixPath
import shutil
import textwrap

import yaml


def navigation(value, docs):
    if not isinstance(value, list) or not value:
        raise ValueError("Experimental navigation must be a non-empty list")
    for item in value:
        if not isinstance(item, dict) or len(item) != 1:
            raise ValueError("Each navigation entry needs one title")
        title, target = next(iter(item.items()))
        if not isinstance(title, str) or not title.strip():
            raise ValueError("Navigation titles must be text")
        if isinstance(target, list):
            navigation(target, docs)
        elif isinstance(target, str):
            path = PurePosixPath(target)
            if ("\\" in target or path.is_absolute() or ".." in path.parts
                    or not target.startswith("experimental/") or path.suffix != ".md"
                    or not (docs / target).is_file()):
                raise ValueError(f"Experimental page is missing or outside its area: {target}")
        else:
            raise ValueError("Navigation targets must be pages or groups")


def checked_tree(folder):
    if not folder.is_dir() or folder.is_symlink():
        raise ValueError(f"Missing or redirected documentation folder: {folder}")
    for path in folder.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"Documentation symlink is not supported: {path}")


def prepare(stable, experimental, output, stable_sha="unknown", experimental_sha="unknown"):
    stable, experimental, output = (Path(p).resolve() for p in (stable, experimental, output))
    for folder in (stable / "docs", experimental / "docs"):
        checked_tree(folder)
        if output == folder or output.is_relative_to(folder):
            raise ValueError("Output must be outside the input documentation folders")
    if output.exists():
        raise ValueError("Output already exists; choose a fresh staging folder")
    area = experimental / "docs/experimental"
    checked_tree(area)
    entries = json.loads((experimental / "build/docs-experimental-nav.json").read_text(encoding="utf-8"))
    navigation(entries, experimental / "docs")
    config = (stable / "mkdocs.yml").read_text(encoding="utf-8")
    root = yaml.compose(config)
    pairs = [(key, value) for key, value in root.value if key.value == "nav"]
    if len(pairs) != 1 or not isinstance(pairs[0][1], yaml.SequenceNode):
        raise ValueError("Stable configuration needs one navigation list")
    key, node = pairs[0]
    if key.start_mark.column:
        raise ValueError("Navigation must be a top-level configuration key")
    nav = yaml.safe_load(yaml.serialize(node))
    nav = [row for row in nav if not (isinstance(row, dict) and "Experimental" in row)]
    nav.append({"Experimental": entries})
    replacement = "nav:\n" + textwrap.indent(yaml.safe_dump(nav, sort_keys=False, allow_unicode=True), "  ")
    config = config[:key.start_mark.index] + replacement + config[node.end_mark.index:]
    output.mkdir(parents=True)
    shutil.copytree(stable / "docs", output / "docs", ignore=lambda folder, names:
                    ["experimental"] if Path(folder) == stable / "docs" else [])
    shutil.copytree(area, output / "docs/experimental")
    # Diagnostic URLs are shared by every released app. Keep old anchors and add
    # new codes here so experimental logs retain working links to the public site.
    for relative in ("ERROR-CODES.md", "assets/data/diagnostic-codes.json"):
        source = experimental / "docs" / relative
        if not source.is_file() or source.is_symlink():
            raise ValueError(f"Missing generated diagnostic reference: {relative}")
        target = output / "docs" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    metadata = {"stable": {"branch": "main", "commit": stable_sha},
                "experimental": {"branch": "experimental", "commit": experimental_sha,
                                 "path": "experimental/"}}
    (output / "docs/assets/data/documentation-channels.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    (output / "mkdocs.yml").write_text(config, encoding="utf-8")
    return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stable", type=Path, required=True)
    parser.add_argument("--experimental", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stable-sha", default="unknown")
    parser.add_argument("--experimental-sha", default="unknown")
    args = parser.parse_args()
    print(json.dumps(prepare(args.stable, args.experimental, args.output, args.stable_sha, args.experimental_sha)))
