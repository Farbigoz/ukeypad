#!/usr/bin/env python3
"""Assemble the dependency-free Web Serial configurator."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from html.parser import HTMLParser
from pathlib import Path

JS_FILES = (
    "state.js",
    "device-view.js",
    "bindings-view.js",
    "ui.js",
    "events-view.js",
    "cdc-transport.js",
    "app.js",
)

STYLE_MARKER = "<!-- __STYLE__ -->"
SCRIPT_MARKER = "<!-- __SCRIPT__ -->"
PROVENANCE = "<!-- Generated from docs/src/configurator/; edit sources, not this file. -->"


class ValidationParser(HTMLParser):
    """HTML parser used to catch malformed generated markup."""


def read_source(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as error:
        raise SystemExit(f"Unable to read {path}: {error}") from error


def replace_marker(template: str, marker: str, value: str) -> str:
    count = template.count(marker)
    if count != 1:
        raise SystemExit(f"Expected exactly one {marker}, found {count}")
    return template.replace(marker, value)


def assemble(source_dir: Path) -> str:
    template = read_source(source_dir / "template.html")
    style = read_source(source_dir / "style.css")
    js_dir = source_dir / "js"
    script = "(() => {\n  'use strict';\n\n"
    script += "\n\n".join(read_source(js_dir / name).rstrip() for name in JS_FILES)
    script += "\n})();\n"

    output = replace_marker(template, STYLE_MARKER, f"<style>\n{style.rstrip()}\n  </style>")
    output = replace_marker(output, SCRIPT_MARKER, f"<script>\n{script.rstrip()}\n</script>")
    return output.replace("<!doctype html>", f"<!doctype html>\n{PROVENANCE}", 1)


def validate(output: str) -> None:
    parser = ValidationParser()
    parser.feed(output)
    parser.close()

    with tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=".js", delete=False) as handle:
        script_path = Path(handle.name)
        start = output.index("<script>") + len("<script>")
        end = output.index("</script>", start)
        handle.write(output[start:end])

    try:
        try:
            subprocess.run(
                ["node", "--check", str(script_path)],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
        except FileNotFoundError:
            print("warning: node not found; skipped JavaScript syntax check", file=sys.stderr)
        except subprocess.CalledProcessError as error:
            print(error.stderr, file=sys.stderr, end="")
            raise SystemExit("Generated configurator JavaScript failed syntax check") from error
    finally:
        script_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, default=Path("docs/src/configurator"))
    parser.add_argument("--out", type=Path, default=Path("docs/configurator.html"))
    parser.add_argument("--check", action="store_true", help="fail if the committed output is stale")
    parser.add_argument("--no-validate", action="store_true", help="skip HTML and JavaScript validation")
    args = parser.parse_args()

    output = assemble(args.src)
    if not args.no_validate:
        validate(output)

    if args.check:
        if not args.out.exists():
            print(f"stale: generated output is missing: {args.out}", file=sys.stderr)
            return 1
        if args.out.read_text(encoding="utf-8") != output:
            print(f"stale: regenerate {args.out} with this script", file=sys.stderr)
            return 1
        print(f"up to date: {args.out}")
        return 0

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(output, encoding="utf-8", newline="\n")
    print(f"generated: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
