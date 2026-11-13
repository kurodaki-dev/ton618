#!/usr/bin/env python3
"""Adds the copyright notice to the TON618 logo (idempotent).

    python3 scripts/logo.py                 # updates exemples/logo.svg in place
    python3 scripts/logo.py path/to/new.svg # installs a new logo: copies it to
                                            # exemples/logo.svg, with the notice

The notice is written inside the SVG (an XML comment, <title>, <desc> and
Dublin Core <metadata>), so it travels with the file wherever it's copied —
the website, the IDE, the editor extensions. The image itself is untouched.
Run by the release and website workflows too, so a logo committed without
the notice still ships with it.
"""
import re
import sys
from pathlib import Path

HOLDER = "kurodaki-dev"
YEAR = "2026"
NOTICE = f"Copyright © {YEAR} {HOLDER}. All rights reserved."
MARK = "data-ton618-copyright"

ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "exemples" / "logo.svg"


def with_notice(svg: str) -> str:
    if MARK in svg:
        return svg
    m = re.search(r"<svg\b[^>]*>", svg)
    if not m:
        raise SystemExit("not an SVG file: no <svg> element found")
    tag = m.group(0)
    new_tag = tag[:-1].rstrip("/") + f' {MARK}="{NOTICE}">'
    for prefix, uri in (("dc", "http://purl.org/dc/elements/1.1/"),
                        ("rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"),
                        ("cc", "http://creativecommons.org/ns#")):
        if f"xmlns:{prefix}=" not in new_tag:
            new_tag = new_tag[:-1] + f' xmlns:{prefix}="{uri}">'
    block = (
        f"<!-- TON618 logo. {NOTICE} The logo is NOT covered by the Apache 2.0 license "
        "of the TON618 source code: see LOGO-LICENSE.md. -->"
        f"<title>TON618 logo — © {YEAR} {HOLDER}</title>"
        f"<desc>Official logo of the TON618 programming language. {NOTICE} "
        "Not covered by the Apache 2.0 license; see LOGO-LICENSE.md at https://github.com/kurodaki-dev/ton618.</desc>"
        "<metadata><rdf:RDF><cc:Work rdf:about=\"\"><dc:title>TON618 logo</dc:title>"
        f"<dc:creator><cc:Agent><dc:title>{HOLDER}</dc:title></cc:Agent></dc:creator>"
        f"<dc:rights><cc:Agent><dc:title>{NOTICE}</dc:title></cc:Agent></dc:rights>"
        "<dc:source>https://github.com/kurodaki-dev/ton618</dc:source></cc:Work></rdf:RDF></metadata>"
    )
    return svg[:m.start()] + new_tag + block + svg[m.end():]


def main() -> None:
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else TARGET
    svg = source.read_text(encoding="utf-8")
    out = with_notice(svg)
    TARGET.write_text(out, encoding="utf-8")
    print(("installed " + str(source) + " as " if source != TARGET else "") +
          f"{TARGET.relative_to(ROOT)}: copyright notice {'added' if out != svg else 'already present'}")


if __name__ == "__main__":
    main()
