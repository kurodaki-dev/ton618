"""The TON618 documentation website — a Claide app (https://github.com/kurodaki-dev/Claide).

Every page is built in Python with Claide: HTML tags are Python functions
(`t.section(...)`, `t.a(...)`), pages are `Page` objects, URLs are routes.
The text comes from the official reference, DOCUMENTATION.md: each "###"
section of its English and French halves becomes one page, so the website
always says exactly what the reference says.

    pip install claide-web
    python website/app.py            # local preview, http://127.0.0.1:8000
    python website/app.py build out  # static site in out/ (what GitHub Pages serves)
"""

from __future__ import annotations

import html
import os
import re
import shutil
import sys
import unicodedata
from dataclasses import dataclass, field
from pathlib import Path

from claide import App, Page, abort, raw, send_file, tags as t
from claide.markdown import render as markdown_to_html

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import registry as package_registry  # noqa: E402  (the same code that checks and indexes the packages)

ROOT = Path(__file__).resolve().parent.parent
REPO = "https://github.com/kurodaki-dev/ton618"
LATEST = REPO + "/releases/latest"
VERSION = re.search(r'TON618_VERSION "([^"]+)"', (ROOT / "include/Version.hpp").read_text()).group(1)
HIGHLIGHT = "https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"
FONTS = ("https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;500;600;700"
         "&family=IBM+Plex+Mono:wght@400;500;600&display=swap")

app = App(__name__, title="TON618", base_path=os.environ.get("BASE_PATH", ""))
app.css(FONTS, app.static("site.css"))
app.js(HIGHLIGHT, app.static("site.js"))
link = app.url  # every internal URL goes through it, so the site also works under a sub-path


# ===========================================================================
# Words of the interface, in English and in French
# ===========================================================================
TEXT = {
    "en": {
        "root": "", "other": "fr", "switch": "Français",
        "home": "Home", "docs": "Documentation", "playground": "Playground", "download": "Download",
        "skills": "AI skills", "source": "Source code", "search": "Search", "nothing": "No match",
        "menu": "Menu", "on_page": "On this page", "prev": "Previous", "next": "Next",
        "edit": "Suggest a change to this page", "all": "All chapters",
        "lead": "A small interpreted programming language, written in C++ and shipped as a single binary.",
        "pitch": ("Classes, arrow functions, f-strings, ranges and a standard library; an HTTP server, a debugger "
                  "and a REPL. One binary, no dependencies, on Linux, Windows, macOS and Android."),
        "install": "Install", "then": "Then:", "start": "Start here",
        "sample": "A taste of TON618", "tools": "Tools", "tools_note": "Free downloads, built from this repository.",
        "missing": "This page doesn't exist.", "back": "Back to the documentation",
        "footer": "Documentation generated from DOCUMENTATION.md with Claide. Code: Apache 2.0. Logo © 2026 kurodaki-dev.",
    },
    "fr": {
        "root": "/fr", "other": "en", "switch": "English",
        "home": "Accueil", "docs": "Documentation", "playground": "Playground", "download": "Télécharger",
        "skills": "Skills IA", "source": "Code source", "search": "Rechercher", "nothing": "Aucun résultat",
        "menu": "Menu", "on_page": "Sur cette page", "prev": "Précédent", "next": "Suivant",
        "edit": "Proposer une modification de cette page", "all": "Tous les chapitres",
        "lead": "Un petit langage de programmation interprété, écrit en C++ et livré en un seul binaire.",
        "pitch": ("Classes, fonctions fléchées, f-strings, intervalles et une bibliothèque standard ; un serveur HTTP, "
                  "un débogueur et un REPL. Un seul binaire, sans dépendances, sur Linux, Windows, macOS et Android."),
        "install": "Installation", "then": "Ensuite :", "start": "Pour commencer",
        "sample": "Un avant-goût de TON618", "tools": "Outils", "tools_note": "Téléchargements gratuits, compilés depuis ce dépôt.",
        "missing": "Cette page n'existe pas.", "back": "Retour à la documentation",
        "footer": "Documentation générée depuis DOCUMENTATION.md avec Claide. Code : Apache 2.0. Logo © 2026 kurodaki-dev.",
    },
}

# Chapters are grouped in the menu. (first section number, English name, French name)
PARTS = [
    (0, "Getting started", "Premiers pas"),
    (4, "The language", "Le langage"),
    (17, "Modules", "Modules"),
    (21, "Tools", "Outils"),
    (24, "Internals", "Fonctionnement"),
]

# "Start here" on the home page: section numbers of the first chapters to read.
START = [1, 4, 9, 12]

TOOLS = [
    ("TON618 IDE", "Windows · Linux · macOS · Android", "doc:23"),
    ("VS Code", ".vsix", "doc:23"),
    ("Acode", "Android", "doc:23"),
    ("Playground", "browser", "/playground.html"),
]

SAMPLE = '''ton.class Counter {
    ton.int value = 0
    ton.function add(n = 1) {
        self.value += n
        return self
    }
}

ton.let words = "a black hole is a very big object".split(" ")
ton.let seen = Counter()
for w in words {
    seen.add()
}
print(f"{seen.value} words, {words.unique().len()} different")
print(words.sorted(w => w.len()) |> join(", "))'''


# ===========================================================================
# DOCUMENTATION.md -> chapters
# ===========================================================================
def slug(text: str) -> str:
    """"Opérateurs" -> "operateurs", "Classes & objects" -> "classes-objects"."""
    text = unicodedata.normalize("NFKD", text.replace("`", ""))
    text = "".join(ch for ch in text if not unicodedata.combining(ch)).lower()
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-")


def heading_id(text: str) -> str:
    """The id Claide's Markdown renderer gives a heading."""
    return re.sub(r"[^a-z0-9]+", "-", re.sub(r"<[^>]+>|`", "", text).lower()).strip("-")


@dataclass
class Chapter:
    lang: str
    number: int
    title: str
    source: str
    body: str = ""
    sections: list = field(default_factory=list)  # [(id, title)] of its "##" headings
    plain: str = ""

    @property
    def slug(self) -> str:
        return slug(self.title)

    @property
    def url(self) -> str:
        return link(f"{TEXT[self.lang]['root']}/docs/{self.slug}/")

    @property
    def part(self) -> str:
        start, en, fr = [p for p in PARTS if p[0] <= self.number][-1]
        return en if self.lang == "en" else fr


class Reference:
    """The two halves of DOCUMENTATION.md, cut into chapters, plus a map from
    the anchors the file links to (GitHub's heading ids) to chapter pages."""

    def __init__(self, path: Path) -> None:
        text = path.read_text(encoding="utf-8")
        cut = text.index("## 🇫🇷 Français")
        self.chapters = {"en": [], "fr": []}
        self.anchors: dict = {}
        self._seen: dict = {}
        for lang, half in (("en", text[:cut]), ("fr", text[cut:])):
            self._split(lang, half)
        if len(self.chapters["en"]) != len(self.chapters["fr"]):
            raise SystemExit("DOCUMENTATION.md: the English and French halves must have the same sections")
        for lang in self.chapters:
            for chapter in self.chapters[lang]:
                self._render(chapter)

    def _github_id(self, text: str) -> str:
        s = re.sub(r"[^\w\- ]", "", text.strip().lower().replace("`", "")).replace(" ", "-")
        n = self._seen.get(s, 0)
        self._seen[s] = n + 1
        return s if n == 0 else f"{s}-{n}"

    def _split(self, lang: str, half: str) -> None:
        title, lines, in_code = None, [], False
        chapters = self.chapters[lang]

        def close():
            if title is not None:
                chapters.append(Chapter(lang, len(chapters), title, "\n".join(lines)))

        for line in half.splitlines():
            if line.startswith("```"):
                in_code = not in_code
            heading = None if in_code else re.match(r"(#{3,6})\s+(.*)", line)
            if heading:
                level, text = len(heading.group(1)), heading.group(2).strip()
                anchor = self._github_id(text)
                if level == 3:
                    close()
                    title, lines = (None, []) if text in ("Table of contents", "Table des matières") else (text, [])
                    if title:
                        self.anchors[anchor] = (lang, len(chapters), "")
                    continue
                if title:
                    self.anchors[anchor] = (lang, len(chapters), heading_id(text))
            if title is not None:
                lines.append(line)
        close()

    def _resolve(self, here: Chapter, target: str) -> str:
        """Where a link of DOCUMENTATION.md should point on the website."""
        if target.startswith("#"):
            found = self.anchors.get(target[1:])
            if not found:
                return target
            lang, number, frag = found
            page = self.chapters[lang][number]
            return ("" if page is here else page.url) + (f"#{frag}" if frag else ("#" if page is here else ""))
        if re.match(r"^[a-z]+:", target) or target.startswith("/"):
            return target
        path, _, frag = target.partition("#")
        if path.startswith("exemples/") and path.endswith(".html"):
            return link("/" + path.split("/", 1)[1])
        return f"{REPO}/blob/main/{path}" + (f"#{frag}" if frag else "")

    def _render(self, chapter: Chapter) -> None:
        bar = "\x01"  # stands for an escaped "\|" in a table while Markdown is rendered
        out, in_code = [], False
        for line in chapter.source.splitlines():
            if line.startswith("```"):
                in_code = not in_code
                if in_code and line.strip() == "```":
                    line = "```ton"  # untagged examples are TON618 code
            elif not in_code:
                if line.strip() == "---":
                    continue
                deeper = re.match(r"(#{4,6})\s+(.*)", line)
                if deeper:  # the chapter title is the page's <h1>: #### becomes ##
                    line = "#" * (len(deeper.group(1)) - 2) + " " + deeper.group(2)
                line = line.replace("\\|", bar)
            out.append(line)
        _, body, sections = markdown_to_html("\n".join(out), lambda target: self._resolve(chapter, target))
        chapter.body = str(body).replace(bar, "|")
        chapter.sections = [(i, s.replace(bar, "|")) for i, s in sections]
        chapter.plain = html.unescape(re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", chapter.body))).strip()


DOCS = Reference(ROOT / "DOCUMENTATION.md")

# The registry: every published package version (registry/packages/*.tonpkg).
PACKAGE_FILES, _ = package_registry.load(ROOT / "registry" / "packages")
PACKAGE_INDEX = package_registry.build_index(PACKAGE_FILES)["packages"]
VERSIONS = {}
for _p in PACKAGE_FILES:
    VERSIONS.setdefault(_p.name, {})[_p.version] = _p

TEXT["en"].update(packages="Packages")
TEXT["fr"].update(packages="Paquets")
PKG = {
    "en": {
        "title": "Packages", "lead": "Modules published for TON618. Install one with a single command.",
        "filter": "Filter by name or keyword", "none": "No package matches.", "latest": "Latest",
        "install": "Install", "details": "Details", "license": "License", "author": "Author", "homepage": "Homepage",
        "requires": "Needs ton618", "needs": "Needs packages", "versions": "Versions", "download": "Download",
        "readme": "README", "no_readme": "This package has no README.", "publish_title": "Publish your own",
        "publish": "A package is a folder with a tonproject.toml. Three commands share it with everyone:",
        "guide": "Read the guide", "count": "packages", "count1": "package", "back": "All packages",
        "caution": "A package runs with the full power of ton618. Read what you install.",
    },
    "fr": {
        "title": "Paquets", "lead": "Les modules publiés pour TON618. On en installe un avec une seule commande.",
        "filter": "Filtrer par nom ou mot-clé", "none": "Aucun paquet ne correspond.", "latest": "Dernière",
        "install": "Installer", "details": "Détails", "license": "Licence", "author": "Auteur", "homepage": "Site",
        "requires": "Demande ton618", "needs": "Demande les paquets", "versions": "Versions", "download": "Télécharger",
        "readme": "README", "no_readme": "Ce paquet n'a pas de README.", "publish_title": "Publier le tien",
        "publish": "Un paquet est un dossier avec un tonproject.toml. Trois commandes le partagent avec tout le monde :",
        "guide": "Lire le guide", "count": "paquets", "count1": "paquet", "back": "Tous les paquets",
        "caution": "Un paquet s'exécute avec toute la puissance de ton618. Lis ce que tu installes.",
    },
}
PACKAGES_CHAPTER = {lang: next(c for c in DOCS.chapters[lang] if c.slug.startswith(("packages", "paquets")))
                    for lang in ("en", "fr")}


# ===========================================================================
# Building blocks
# ===========================================================================
def brand(lang: str):
    return t.a(
        t.img(src=link("/logo.svg"), alt="", width="34", height="34"),
        t.span(t.b("TON"), "618"),
        href=link(TEXT[lang]["root"] + "/"), class_="brand", aria_label="TON618",
    )


def menu(lang: str, current: Chapter | None):
    """The side menu: search, then the chapters by part (the current part open)."""
    w = TEXT[lang]
    chapters = DOCS.chapters[lang]
    parts = []
    for i, (start, en, fr) in enumerate(PARTS):
        end = PARTS[i + 1][0] if i + 1 < len(PARTS) else len(chapters)
        inside = chapters[start:end]
        is_open = current is None and i == 0 or current is not None and current in inside
        parts.append(t.details(
            t.summary(en if lang == "en" else fr),
            t.ol([t.li(t.a(c.title, href=c.url, aria_current="page" if c is current else None)) for c in inside],
                 start=str(start + 1)),
            open=is_open,
        ))
    return t.nav(
        t.div(
            t.input(type="search", id="find", placeholder=w["search"] + "  /", autocomplete="off",
                    aria_label=w["search"], data_source=link(w["root"] + "/search.json"), data_none=w["nothing"]),
            t.ul(id="found", hidden=True),
            class_="finder",
        ),
        *parts,
        class_="chapters", aria_label=w["docs"],
    )


def sidebar(lang: str, current: Chapter | None, switch_to: str):
    w = TEXT[lang]
    return t.aside(
        t.div(brand(lang), t.span(VERSION, class_="version"), class_="side-top"),
        menu(lang, current),
        t.div(
            t.a(w["packages"], href=link(w["root"] + "/packages/")),
            t.a(w["playground"], href=link("/playground.html")),
            t.a(w["download"], href=LATEST),
            t.a(w["skills"], href=link("/skills.html")),
            t.a(w["source"], href=REPO),
            t.a(w["switch"], href=switch_to, hreflang=w["other"], lang=w["other"], class_="switch"),
            class_="side-links",
        ),
        id="side", class_="side",
    )


def shell(lang: str, title: str, content, *, current: Chapter | None = None, switch_to: str = "",
          summary: str = "") -> Page:
    """Every page: the side menu, a top bar for small screens, the content and the footer."""
    w = TEXT[lang]
    other = TEXT[w["other"]]
    return Page(
        title,
        t.div(
            sidebar(lang, current, switch_to or link(other["root"] + "/")),
            t.div(
                t.header(
                    t.button("☰ " + w["menu"], type="button", id="menu-button", aria_controls="side",
                             aria_expanded="false"),
                    brand(lang),
                    class_="topbar",
                ),
                t.main(content, id="content"),
                t.footer(t.p(w["footer"]), class_="foot"),
                class_="page",
            ),
            class_="layout",
        ),
        lang=lang,
        description=summary or w["lead"],
        favicon=link("/logo.svg"),
        body_attrs={"data-root": link("/").rstrip("/"), "data-lang": lang},
    )


def code(source: str, language: str = "ton", label: str = ""):
    block = t.pre(t.code(source.strip("\n"), class_=f"language-{language}"))
    return t.figure(t.figcaption(label), block, class_="listing") if label else block


# ===========================================================================
# Pages
# ===========================================================================
def home(lang: str) -> Page:
    w = TEXT[lang]
    chapters = DOCS.chapters[lang]

    def tool_link(target: str) -> str:
        return chapters[int(target[4:])].url if target.startswith("doc:") else link(target)

    content = t.div(
        t.section(
            t.img(src=link("/logo.svg"), alt="TON618", width="120", height="120", class_="mark"),
            t.h1("TON618"),
            t.p(w["lead"], class_="lead"),
            t.p(w["pitch"]),
            class_="intro",
        ),
        t.section(
            t.h2(w["install"]),
            t.div(
                code("curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash",
                     "bash", "Linux · macOS · Termux"),
                code('powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"',
                     "powershell", "Windows"),
                class_="pair",
            ),
            t.p(w["then"]),
            code("ton618 hello.ton        # run a file\nton618                  # interactive REPL\nton618 --update         # newest version",
                 "bash"),
        ),
        t.section(
            t.h2(PKG[lang]["publish_title"]),
            t.p(PKG[lang]["publish"]),
            code("ton618 init mathx      # creates tonproject.toml\nton618 build           # -> dist/mathx-0.1.0.tonpkg\nton618 publish         # opens the pull request",
                 "bash"),
            t.p(t.a(PKG[lang]["title"] + " (" + str(len(PACKAGE_INDEX)) + ") →", href=link(w["root"] + "/packages/")), " · ",
                t.a(PKG[lang]["guide"], href=PACKAGES_CHAPTER[lang].url)),
        ),
        t.section(
            t.h2(w["start"]),
            t.ol([t.li(t.a(chapters[n].title, href=chapters[n].url)) for n in START], class_="steps"),
            t.p(t.a(w["all"] + " →", href=link(w["root"] + "/docs/"))),
        ),
        t.section(t.h2(w["sample"]), code(SAMPLE, "ton", "words.ton")),
        t.section(
            t.h2(w["tools"]),
            t.p(w["tools_note"]),
            t.table(t.tbody([t.tr(t.th(t.a(name, href=tool_link(target))), t.td(note))
                             for name, note, target in TOOLS]), class_="tools"),
        ),
        class_="home",
    )
    return shell(lang, "TON618", content, switch_to=link(TEXT[w["other"]]["root"] + "/"))


def contents(lang: str) -> Page:
    w = TEXT[lang]
    chapters = DOCS.chapters[lang]
    blocks = []
    for i, (start, en, fr) in enumerate(PARTS):
        end = PARTS[i + 1][0] if i + 1 < len(PARTS) else len(chapters)
        blocks.append(t.h2(en if lang == "en" else fr))
        blocks.append(t.ol([t.li(t.a(c.title, href=c.url)) for c in chapters[start:end]],
                           start=str(start + 1), class_="steps"))
    return shell(lang, f"{w['docs']} · TON618", t.article(t.h1(w["docs"]), *blocks, class_="doc"),
                 switch_to=link(TEXT[w["other"]]["root"] + "/docs/"))


def chapter_page(chapter: Chapter) -> Page:
    w = TEXT[chapter.lang]
    chapters = DOCS.chapters[chapter.lang]
    before = chapters[chapter.number - 1] if chapter.number > 0 else None
    after = chapters[chapter.number + 1] if chapter.number + 1 < len(chapters) else None
    twin = DOCS.chapters[w["other"]][chapter.number]
    article = t.article(
        t.p(t.a(w["docs"], href=link(w["root"] + "/docs/")), " / ", chapter.part, class_="trail"),
        t.h1(chapter.title),
        t.details(
            t.summary(w["on_page"]),
            t.ul([t.li(t.a(title, href=f"#{anchor}")) for anchor, title in chapter.sections]),
            class_="outline",
        ) if len(chapter.sections) > 1 else None,
        raw(chapter.body),
        t.nav(
            t.a(t.small(w["prev"]), before.title, href=before.url, rel="prev") if before else t.span(),
            t.a(t.small(w["next"]), after.title, href=after.url, rel="next") if after else t.span(),
            class_="turn", aria_label=w["docs"],
        ),
        t.p(t.a(w["edit"], href=f"{REPO}/edit/main/DOCUMENTATION.md"), class_="edit"),
        class_="doc",
    )
    return shell(chapter.lang, f"{chapter.title} · TON618", article, current=chapter,
                 switch_to=twin.url, summary=chapter.plain[:155])


# ===========================================================================
# Packages
# ===========================================================================
def safe_link(target: str) -> str:
    """Links in a package's README: web addresses and anchors only (never javascript: and co)."""
    if target.startswith("#") or re.match(r"^(https?://|mailto:)", target, re.I):
        return target
    return "#"


def readme_html(name: str) -> str:
    latest = VERSIONS[name][PACKAGE_INDEX[name]["latest"]]
    files = latest.data.get("files", {})
    text = files.get("README.md") or next((v for k, v in sorted(files.items()) if k.lower().endswith(".md")), "")
    if not text:
        return ""
    lines = text.splitlines()
    if lines and lines[0].startswith("# "):  # the page already has the package's name as its title
        lines = lines[1:]
    _, body, _ = markdown_to_html("\n".join(lines), safe_link)
    return str(body)


def packages_page(lang: str) -> Page:
    w, k = TEXT[lang], PKG[lang]
    rows = []
    for name, info in PACKAGE_INDEX.items():
        keywords = " ".join(info.get("keywords") or [])
        rows.append(t.li(
            t.div(t.a(name, href=link(f"{w['root']}/packages/{name}/"), class_="pkg-name"),
                  t.span(info["latest"], class_="pkg-version")),
            t.p(info["description"]),
            t.code(f"ton618 install {name}"),
            class_="pkg", data_text=f"{name} {info['description']} {keywords}".lower(),
        ))
    count = len(rows)
    content = t.article(
        t.h1(k["title"]),
        t.p(k["lead"], class_="lead"),
        t.input(type="search", id="pkg-filter", placeholder=k["filter"], aria_label=k["filter"]),
        t.p(f"{count} {k['count1'] if count == 1 else k['count']}", class_="pkg-count"),
        t.ul(rows, class_="pkg-list", id="pkg-list"),
        t.p(k["none"], id="pkg-none", hidden=True),
        t.h2(k["publish_title"]),
        t.p(k["publish"]),
        code("ton618 init mathx\nton618 build\nton618 publish", "bash"),
        t.p(t.a(k["guide"] + " →", href=PACKAGES_CHAPTER[lang].url)),
        class_="doc",
    )
    return shell(lang, f"{k['title']} · TON618", content,
                 switch_to=link(f"{TEXT[w['other']]['root']}/packages/"), summary=k["lead"])


def package_page(lang: str, name: str) -> Page:
    w, k = TEXT[lang], PKG[lang]
    info = PACKAGE_INDEX[name]
    versions = sorted(VERSIONS[name].values(), key=lambda p: package_registry.version_key(p.version), reverse=True)
    latest = VERSIONS[name][info["latest"]]
    deps = latest.data.get("dependencies") or {}

    def row(label, value):
        return t.tr(t.th(label), t.td(value)) if value else None

    homepage = info.get("homepage") or ""
    details = t.table(t.tbody([
        row(k["latest"], info["latest"]),
        row(k["license"], info.get("license")),
        row(k["author"], info.get("author")),
        row(k["homepage"], t.a(homepage, href=homepage) if re.match(r"^https?://", homepage) else None),
        row(k["requires"], t.code(latest.data["requires"]) if latest.data.get("requires") else None),
        row(k["needs"], t.span([t.span(t.a(d, href=link(f"{w['root']}/packages/{d}/")), t.code(" " + r), " ") for d, r in deps.items()])
            if deps else None),
    ]), class_="facts")
    history = t.table(
        t.thead(t.tr(t.th(k["versions"]), t.th("sha256"), t.th(""))),
        t.tbody([t.tr(
            t.td(t.code(p.version)),
            t.td(t.code(p.sha256[:12] + "…")),
            t.td(t.a(k["download"], href=link("/registry/" + p.file))),
        ) for p in versions]),
        class_="facts",
    )
    readme = readme_html(name)
    content = t.article(
        t.p(t.a(k["back"], href=link(f"{w['root']}/packages/")), class_="trail"),
        t.h1(name, " ", t.span(info["latest"], class_="pkg-version")),
        t.p(info["description"], class_="lead"),
        code(f"ton618 install {name}", "bash"),
        details,
        history,
        t.p(k["caution"], class_="callout"),
        t.h2(k["readme"]),
        raw(readme) if readme else t.p(k["no_readme"]),
        class_="doc",
    )
    return shell(lang, f"{name} · {k['title']} · TON618", content,
                 switch_to=link(f"{TEXT[w['other']]['root']}/packages/{name}/"), summary=info["description"])


# ===========================================================================
# Routes
# ===========================================================================
for lang in ("en", "fr"):
    root = TEXT[lang]["root"]
    app.add_route(root + "/" if root else "/", (lambda l: lambda: home(l))(lang), name=f"home_{lang}")
    app.add_route(root + "/docs/", (lambda l: lambda: contents(l))(lang), name=f"contents_{lang}")
    app.add_route(root + "/search.json",
                  (lambda l: lambda: [{"t": c.title, "u": c.url, "x": c.plain[:5000]} for c in DOCS.chapters[l]])(lang),
                  name=f"search_{lang}")
    app.add_route(root + "/packages/", (lambda l: lambda: packages_page(l))(lang), name=f"packages_{lang}")
    for package_name in PACKAGE_INDEX:
        app.add_route(f"{root}/packages/{package_name}/", (lambda l, n: lambda: package_page(l, n))(lang, package_name),
                      name=f"package_{lang}_{package_name}")
    for chapter in DOCS.chapters[lang]:
        app.add_route(f"{root}/docs/{chapter.slug}/", (lambda c: lambda: chapter_page(c))(chapter),
                      name=f"doc_{lang}_{chapter.number}")


@app.get("/registry/index.json")
def registry_index():
    """Local preview only: what GitHub Pages serves as /registry/index.json."""
    return package_registry.build_index(PACKAGE_FILES)


@app.get("/registry/packages/<name>")
def registry_package(name):
    target = ROOT / "registry" / "packages" / name
    if not target.is_file() or target.suffix != ".tonpkg":
        abort(404)
    return send_file(str(target))


@app.get("/<path:name>")
def exemples(name):
    """Local preview only: the files GitHub Pages gets from exemples/ (playground, logo...)."""
    folder = (ROOT / "exemples").resolve()
    target = (folder / name).resolve()
    if folder not in target.parents or not target.is_file():
        abort(404)
    return send_file(str(target))


@app.errorhandler(404)
def missing():
    return shell("en", "404 · TON618", t.article(
        t.h1("404"), t.p(TEXT["en"]["missing"], " · ", TEXT["fr"]["missing"]),
        t.p(t.a(TEXT["en"]["back"], href=link("/docs/")), " · ", t.a(TEXT["fr"]["back"], href=link("/fr/docs/"))),
        class_="doc",
    ))


# ===========================================================================
# Static export
# ===========================================================================
PUBLISHED = ["playground.html", "ton618-lite.js", "skills.html", "skills", "logo.svg", "CNAME", "documentation.html"]


def export(out: str) -> None:
    pages = app.build(out)
    package_registry.write_site(PACKAGE_FILES, Path(out))  # /registry/index.json and the packages
    for name in PUBLISHED:  # served as they are, next to the generated pages
        source = ROOT / "exemples" / name
        if source.is_dir():
            shutil.copytree(source, Path(out) / name, dirs_exist_ok=True)
        elif source.exists():
            shutil.copy2(source, Path(out) / name)
    Path(out, ".nojekyll").touch()
    print(f"{len(pages)} pages written to {out}")


if __name__ == "__main__":
    if sys.argv[1:2] == ["build"]:
        export(sys.argv[2] if len(sys.argv) > 2 else str(Path(__file__).parent / "_site"))
    else:
        app.run()
