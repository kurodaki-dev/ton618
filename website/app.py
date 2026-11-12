"""TON618's documentation website, built with Claide (https://github.com/kurodaki-dev/Claide).

The content is the official DOCUMENTATION.md, read at build time: every
"### Section" of the English and the French half becomes its own page, so the
site never drifts from the reference document.

Run locally:      pip install claide-web && python website/app.py   -> http://127.0.0.1:8000
Static export:    python website/app.py build _site                 -> _site/ (what GitHub Pages serves)
"""

from __future__ import annotations

import html as htmllib
import json
import os
import re
import shutil
import sys
import unicodedata
from pathlib import Path

from claide import App, Element, Page, abort, raw, send_file, tags as t
from claide.markdown import render as render_markdown

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
REPO = "https://github.com/kurodaki-dev/ton618"
RELEASES = REPO + "/releases"
HLJS = "https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0"
FONTS = ("https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600&"
         "family=Inter:wght@400;500;600;700&display=swap")

VERSION = re.search(r'TON618_VERSION "([^"]+)"', (ROOT / "include" / "Version.hpp").read_text()).group(1)

app = App(__name__, title="TON618", base_path=os.environ.get("BASE_PATH", ""))
app.css(FONTS, f"{HLJS}/styles/github-dark.min.css", app.static("style.css"))
app.js(f"{HLJS}/highlight.min.js", app.static("main.js"))
url = app.url

# ---------------------------------------------------------------------------
# Text of the interface, in both languages
# ---------------------------------------------------------------------------
UI = {
    "en": {
        "prefix": "",
        "docs": "Documentation", "playground": "Playground", "skills": "AI skills", "github": "GitHub",
        "tagline": "A small, fast, friendly programming language — written in C++, shipped as one binary.",
        "lead": ("Write a <code>.ton</code> file and run it. Classes, arrow functions, f-strings, ranges, "
                 "a standard library, an HTTP server, a debugger and a REPL — with no dependencies, "
                 "on Linux, Windows and Android (Termux)."),
        "read": "Read the documentation", "try": "Try it in your browser",
        "install": "Install", "linux": "Linux / Termux", "windows": "Windows",
        "then": "Then run a script, or type <code>ton618</code> alone for the interactive REPL.",
        "features": "What you get",
        "new": "New in 2.0", "new_lead": "The biggest release so far: a modern syntax and an interpreter about six times faster.",
        "chapters": "Chapters", "contents": "Contents", "on_page": "On this page",
        "prev": "Previous", "next": "Next", "edit": "Edit this page on GitHub",
        "search": "Search the docs…", "no_result": "No result",
        "docs_lead": "The complete reference, from installing the interpreter to extending it.",
        "footer": "TON618 is free software under the Apache 2.0 license. This site is built with",
        "footer2": "from", "notfound": "Page not found", "home": "Go to the home page",
        "run": "Run", "lang_switch": "Français", "copy": "Copy", "copied": "Copied!",
    },
    "fr": {
        "prefix": "/fr",
        "docs": "Documentation", "playground": "Playground", "skills": "Skills IA", "github": "GitHub",
        "tagline": "Un petit langage de programmation rapide et accessible — écrit en C++, livré en un seul binaire.",
        "lead": ("Écris un fichier <code>.ton</code> et lance-le. Classes, fonctions fléchées, f-strings, intervalles, "
                 "une bibliothèque standard, un serveur HTTP, un débogueur et un REPL — sans dépendances, "
                 "sur Linux, Windows et Android (Termux)."),
        "read": "Lire la documentation", "try": "L'essayer dans le navigateur",
        "install": "Installation", "linux": "Linux / Termux", "windows": "Windows",
        "then": "Ensuite lance un script, ou tape <code>ton618</code> tout seul pour le REPL interactif.",
        "features": "Ce que tu obtiens",
        "new": "Nouveau en 2.0", "new_lead": "La plus grosse version à ce jour : une syntaxe moderne et un interpréteur environ six fois plus rapide.",
        "chapters": "Chapitres", "contents": "Sommaire", "on_page": "Sur cette page",
        "prev": "Précédent", "next": "Suivant", "edit": "Modifier cette page sur GitHub",
        "search": "Rechercher dans la doc…", "no_result": "Aucun résultat",
        "docs_lead": "La référence complète, de l'installation de l'interpréteur jusqu'à son extension.",
        "footer": "TON618 est un logiciel libre sous licence Apache 2.0. Ce site est construit avec",
        "footer2": "à partir de", "notfound": "Page introuvable", "home": "Retour à l'accueil",
        "run": "Exécuter", "lang_switch": "English", "copy": "Copier", "copied": "Copié !",
    },
}

# Sidebar groups: (first section index, title EN, title FR). Both halves of
# DOCUMENTATION.md list their sections in the same order.
GROUPS = [
    (0, "Getting started", "Premiers pas"),
    (4, "The language", "Le langage"),
    (17, "Modules", "Modules"),
    (21, "Tools", "Outils"),
    (23, "Internals & history", "Fonctionnement & historique"),
]

FEATURES = {
    "en": [
        ("Readable", "Declarations say what they hold: <code>ton.int</code>, <code>ton.string</code>, "
                     "<code>ton.let</code>, <code>ton.const</code>… and errors point at the exact line."),
        ("Object-oriented", "Classes with fields, methods, <code>init</code>, inheritance and <code>super</code> "
                            "— plus method syntax on every value: <code>\"hi\".upper()</code>."),
        ("Functional", "Arrow functions, <code>map</code>/<code>filter</code>/<code>reduce</code>, the pipe "
                       "operator <code>|&gt;</code>, destructuring, ranges and slices."),
        ("Batteries included", "Twelve built-in modules: files, HTTP client, JSON, regex, time, random, "
                               "encoding, paths, strings, math, tensors…"),
        ("Web-ready", "A built-in HTTP server with routes and URL parameters, and an <code>html</code> type "
                      "for pages — no framework needed."),
        ("One binary", "No runtime to install: precompiled for Linux, Windows and Termux, with "
                       "<code>--update</code>, a module installer, a REPL and a debugger."),
    ],
    "fr": [
        ("Lisible", "Les déclarations disent ce qu'elles contiennent : <code>ton.int</code>, <code>ton.string</code>, "
                    "<code>ton.let</code>, <code>ton.const</code>… et les erreurs montrent la ligne exacte."),
        ("Orienté objet", "Des classes avec champs, méthodes, <code>init</code>, héritage et <code>super</code> "
                          "— et la syntaxe méthode sur toutes les valeurs : <code>\"hi\".upper()</code>."),
        ("Fonctionnel", "Fonctions fléchées, <code>map</code>/<code>filter</code>/<code>reduce</code>, l'opérateur "
                        "pipe <code>|&gt;</code>, la déstructuration, les intervalles et les tranches."),
        ("Piles incluses", "Douze modules intégrés : fichiers, client HTTP, JSON, regex, temps, hasard, "
                           "encodage, chemins, chaînes, maths, tenseurs…"),
        ("Prêt pour le web", "Un serveur HTTP intégré avec routes et paramètres d'URL, et un type <code>html</code> "
                             "pour les pages — sans framework."),
        ("Un seul binaire", "Rien à installer à côté : précompilé pour Linux, Windows et Termux, avec "
                            "<code>--update</code>, un installeur de modules, un REPL et un débogueur."),
    ],
}

NEW_IN_2 = {
    "en": [
        ("Classes & objects", "classes-objects"),
        ("Method syntax on every value", "method-syntax-the-dot-operator"),
        ("Arrow functions & the pipe operator", "functions"),
        ("f-strings & raw strings", "string-formatting"),
        ("Ranges, slices & negative indices", "arrays"),
        ("ton.let, ton.const & destructuring", "language-basics"),
        ("Interactive REPL & better errors", "running-a-script"),
        ("~45 new standard functions", "built-in-native-functions"),
    ],
    "fr": [
        ("Classes & objets", "classes-objets"),
        ("La syntaxe méthode sur toutes les valeurs", "syntaxe-methode-l-operateur-point"),
        ("Fonctions fléchées & opérateur pipe", "fonctions"),
        ("f-strings & chaînes brutes", "formatage-de-chaines"),
        ("Intervalles, tranches & indices négatifs", "tableaux"),
        ("ton.let, ton.const & déstructuration", "bases-du-langage"),
        ("REPL interactif & meilleures erreurs", "lancer-un-script"),
        ("~45 nouvelles fonctions standard", "fonctions-natives"),
    ],
}

HERO_CODE = '''ton.class Player {
    ton.int score = 0
    ton.function init(name) { self.name = name }
    ton.function add(points) {
        self.score += points
        return self
    }
}

ton.let players = ["kuro", "rusher", "natlep"].map(n => Player(n))
players[0].add(30).add(12)
players[2].add(25)

for p in players.sorted(p => -p.score) {
    print(f"{p.name.padEnd(8)} {p.score}")
}'''


# ---------------------------------------------------------------------------
# Reading DOCUMENTATION.md
# ---------------------------------------------------------------------------
def slugify(text: str) -> str:
    """URL-friendly slug that keeps accented letters readable: "Opérateurs" -> "operateurs"."""
    text = unicodedata.normalize("NFKD", re.sub(r"<[^>]+>|`", "", text))
    text = "".join(c for c in text if not unicodedata.combining(c)).lower()
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-")


_gh_seen: dict = {}


def github_anchor(text: str) -> str:
    """The anchor GitHub generates for a heading (what DOCUMENTATION.md's own links use)."""
    s = text.strip().lower().replace("`", "")
    s = re.sub(r"[^\w\- ]", "", s).replace(" ", "-")
    n = _gh_seen.get(s, 0)
    _gh_seen[s] = n + 1
    return s if n == 0 else f"{s}-{n}"


class Section:
    def __init__(self, lang: str, index: int, title: str, body: str) -> None:
        self.lang, self.index, self.title, self.body = lang, index, title, body
        self.slug = slugify(title)
        self.html = ""
        self.toc: list = []
        self.text = ""  # plain text, for the search index

    @property
    def href(self) -> str:
        return url(f"{UI[self.lang]['prefix']}/docs/{self.slug}/")


def read_documentation():
    source = (ROOT / "DOCUMENTATION.md").read_text(encoding="utf-8")
    split = source.index("## 🇫🇷 Français")
    halves = {"en": source[:split], "fr": source[split:]}
    sections = {"en": [], "fr": []}
    anchors: dict = {}  # GitHub anchor -> (lang, section index, in-page id or "")
    for lang in ("en", "fr"):
        # Walk the half line by line, outside code blocks, to find headings.
        current = None
        buf: list = []
        in_code = False

        def flush():
            if current is not None:
                sections[lang].append(Section(lang, len(sections[lang]), current, "\n".join(buf)))

        for line in halves[lang].splitlines():
            if line.startswith("```"):
                in_code = not in_code
            if not in_code and line.startswith("#"):
                m = re.match(r"(#+)\s+(.*)", line)
                level, text = len(m.group(1)), m.group(2).strip()
                anchor = github_anchor(text)
                if level == 3:
                    flush()
                    if text in ("Table of contents", "Table des matières"):
                        current, buf = None, []
                        continue
                    current, buf = text, []
                    anchors[anchor] = (lang, len(sections[lang]), "")
                    continue
                if current is not None and level >= 4:
                    anchors[anchor] = (lang, len(sections[lang]), slugify_claide(text))
            if current is not None:
                buf.append(line)
        flush()
    return sections, anchors


def slugify_claide(text: str) -> str:
    """The id Claide's Markdown renderer gives a heading (see claide.markdown.slugify)."""
    text = re.sub(r"<[^>]+>|`", "", text).lower()
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-")


SECTIONS, ANCHORS = read_documentation()
PIPE = "\x01"  # stands for an escaped "\|" inside a table cell while rendering


def render_section(sec: Section) -> None:
    def link(target: str) -> str:
        if target.startswith("#"):
            hit = ANCHORS.get(target[1:])
            if hit:
                lang, idx, frag = hit
                dest = SECTIONS[lang][idx]
                return (dest.href if dest is not sec else "") + (f"#{frag}" if frag else ("" if dest is not sec else "#"))
            return target
        if re.match(r"^[a-z]+:", target) or target.startswith("/"):
            return target
        # A path inside the repository (e.g. .github/workflows/release.yml, DOCUMENTATION.md#x).
        path, _, frag = target.partition("#")
        if path.startswith("exemples/") and path.endswith(".html"):
            return url("/" + path.split("/", 1)[1])
        return f"{REPO}/blob/main/{path}" + (f"#{frag}" if frag else "")

    lines = []
    in_code = False
    for line in sec.body.splitlines():
        if line.startswith("```"):
            in_code = not in_code
            if in_code and line.strip() == "```":
                line = "```ton"  # untagged blocks in the reference are TON618 code
        elif not in_code:
            if line.strip() == "---":
                continue
            m = re.match(r"(#{4,6})\s+(.*)", line)
            if m:  # #### -> ##, ##### -> ###
                line = "#" * (len(m.group(1)) - 2) + " " + m.group(2)
            line = line.replace("\\|", PIPE)
        lines.append(line)
    md = f"# {sec.title}\n\n" + "\n".join(lines)
    _, html, toc = render_markdown(md, link)
    html = str(html).replace(PIPE, "|").replace("<table>", '<table class="ref">')
    sec.html, sec.toc = html, [(i, txt.replace(PIPE, "|")) for i, txt in toc]
    sec.text = htmllib.unescape(re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", html)).strip())


for lang in SECTIONS:
    for s in SECTIONS[lang]:
        render_section(s)

assert len(SECTIONS["en"]) == len(SECTIONS["fr"]), "both halves of DOCUMENTATION.md must have the same sections"


# ---------------------------------------------------------------------------
# Layout
# ---------------------------------------------------------------------------
def other_lang(lang: str) -> str:
    return "fr" if lang == "en" else "en"


def header(lang: str, active: str, switch_href: str) -> Element:
    ui = UI[lang]
    p = ui["prefix"]
    links = [
        (ui["docs"], url(f"{p}/docs/"), "docs"),
        (ui["playground"], url("/playground.html"), "playground"),
        (ui["skills"], url("/skills.html"), "skills"),
        (ui["github"], REPO, ""),
    ]
    return t.header(t.div(
        t.a(t.img(src=url("/logo.svg"), alt="", width="28", height="28"),
            t.span("TON", t.span("618", class_="accent")), href=url(f"{p}/"), class_="wordmark",
            aria_label="TON618"),
        t.a(VERSION, href=url(f"{p}/docs/changelog/"), class_="version"),
        t.nav(t.ul([
            t.li(t.a(label, href=href, aria_current="page" if key and key == active else None))
            for label, href, key in links
        ] + [t.li(t.a(ui["lang_switch"], href=switch_href, class_="lang", hreflang=other_lang(lang),
                      lang=other_lang(lang)))]), aria_label="Main"),
        class_="wrap header-inner",
    ), class_="site-header")


def footer(lang: str) -> Element:
    ui = UI[lang]
    return t.footer(t.div(
        t.p(t.span("TON", t.span("618", class_="accent"), class_="wordmark small"), " ", VERSION),
        t.p(ui["footer"], " ", t.a("Claide", href="https://github.com/kurodaki-dev/Claide"), " ",
            ui["footer2"], " ", t.a("DOCUMENTATION.md", href=f"{REPO}/blob/main/DOCUMENTATION.md"), "."),
        class_="wrap",
    ), class_="site-footer")


def page(lang: str, title: str, *content, active: str = "", description: str = "", switch_href: str = "") -> Page:
    ui = UI[lang]
    full_title = "TON618 — " + ui["tagline"].split(" — ")[0] if not title else f"{title} · TON618"
    p = Page(
        full_title,
        header(lang, active, switch_href or url(f"{UI[other_lang(lang)]['prefix']}/")),
        t.main(*content),
        footer(lang),
        lang=lang,
        description=description or ui["tagline"],
        favicon=url("/logo.svg"),
        body_attrs={"data-base": url("/").rstrip("/"), "data-run": ui["run"],
                    "data-copy": ui["copy"], "data-copied": ui["copied"], "data-lang": lang},
    )
    return p


def code_block(code: str, lang: str = "ton", caption: str = "") -> Element:
    block = t.pre(t.code(code.strip("\n"), class_=f"language-{lang}"))
    if not caption:
        return block
    return t.figure(t.figcaption(caption), block, class_="code-figure")


# ---------------------------------------------------------------------------
# Home pages
# ---------------------------------------------------------------------------
def home(lang: str):
    ui = UI[lang]
    p = ui["prefix"]
    sec_by_slug = {s.slug: s for s in SECTIONS[lang]}
    return page(
        lang, "",
        t.section(t.div(
            t.div(
                t.p(t.span(VERSION, class_="pill"), class_="eyebrow"),
                t.h1("TON", t.span("618", class_="accent")),
                t.p(ui["tagline"], class_="tagline"),
                t.p(raw(ui["lead"]), class_="lead"),
                t.div(
                    t.a(ui["read"], href=url(f"{p}/docs/"), class_="button"),
                    t.a("▶ " + ui["try"], href=url("/playground.html"), class_="button ghost"),
                    class_="actions",
                ),
                class_="intro",
            ),
            code_block(HERO_CODE, caption="leaderboard.ton"),
            class_="wrap hero-grid",
        ), class_="hero"),

        t.section(t.div(
            t.h2(ui["install"]),
            t.div(
                code_block("curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash",
                           "bash", caption=ui["linux"]),
                code_block('powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"',
                           "powershell", caption=ui["windows"]),
                class_="install-grid",
            ),
            t.p(raw(ui["then"]), class_="muted"),
            code_block("ton618 hello.ton\nton618                 # REPL\nton618 -e 'print(2 ** 10)'\nton618 --update", "bash"),
            class_="wrap",
        ), class_="band"),

        t.section(t.div(
            t.h2(ui["features"]),
            t.div([t.div(t.h3(name), t.p(raw(text)), class_="feature") for name, text in FEATURES[lang]],
                  class_="features"),
            class_="wrap",
        ), class_="band"),

        t.section(t.div(
            t.h2(ui["new"]),
            t.p(ui["new_lead"], class_="muted"),
            t.ul([t.li(t.a(label, href=sec_by_slug[slug].href if slug in sec_by_slug else url(f"{p}/docs/")))
                  for label, slug in NEW_IN_2[lang]], class_="new-list"),
            class_="wrap",
        ), class_="band"),
        switch_href=url(f"{UI[other_lang(lang)]['prefix']}/"),
    )


# ---------------------------------------------------------------------------
# Documentation pages
# ---------------------------------------------------------------------------
def sidebar(lang: str, current: int = -1) -> Element:
    ui = UI[lang]
    secs = SECTIONS[lang]
    groups = []
    for gi, (start, title_en, title_fr) in enumerate(GROUPS):
        end = GROUPS[gi + 1][0] if gi + 1 < len(GROUPS) else len(secs)
        groups.append(t.div(
            t.p(title_en if lang == "en" else title_fr, class_="group-title"),
            t.ul([t.li(t.a(s.title, href=s.href, aria_current="page" if s.index == current else None))
                  for s in secs[start:end]]),
            class_="group",
        ))
    return t.aside(
        t.button(ui["contents"], type="button", class_="sidebar-toggle", aria_expanded="false"),
        t.div(
            t.input(type="search", class_="search", placeholder=ui["search"], aria_label=ui["search"],
                    data_index=url(f"{ui['prefix']}/search.json"), data_empty=ui["no_result"]),
            t.ul(class_="search-results", hidden=True),
            t.nav(groups, class_="sidebar-nav", aria_label=ui["docs"]),
            class_="sidebar-inner",
        ),
        class_="sidebar",
    )


def docs_index(lang: str):
    ui = UI[lang]
    secs = SECTIONS[lang]
    blocks = []
    for gi, (start, title_en, title_fr) in enumerate(GROUPS):
        end = GROUPS[gi + 1][0] if gi + 1 < len(GROUPS) else len(secs)
        blocks.append(t.h2(title_en if lang == "en" else title_fr))
        blocks.append(t.ol([t.li(t.a(s.title, href=s.href)) for s in secs[start:end]],
                           start=str(start + 1), class_="chapters"))
    return page(lang, ui["docs"], t.div(
        sidebar(lang),
        t.article(t.h1(ui["docs"]), t.p(ui["docs_lead"], class_="lead"), *blocks, class_="doc"),
        class_="wrap docs",
    ), active="docs", switch_href=url(f"{UI[other_lang(lang)]['prefix']}/docs/"))


def doc_page(lang: str, index: int):
    ui = UI[lang]
    secs = SECTIONS[lang]
    sec = secs[index]
    prev_s = secs[index - 1] if index > 0 else None
    next_s = secs[index + 1] if index + 1 < len(secs) else None
    twin = SECTIONS[other_lang(lang)][index]

    def view():
        return page(lang, sec.title, t.div(
            sidebar(lang, index),
            t.article(
                raw(sec.html),
                t.p(t.a(ui["edit"], href=f"{REPO}/edit/main/DOCUMENTATION.md"), class_="edit"),
                t.nav(
                    t.a(t.small(ui["prev"]), t.span(prev_s.title), href=prev_s.href, class_="pager-prev")
                    if prev_s else t.span(),
                    t.a(t.small(ui["next"]), t.span(next_s.title), href=next_s.href, class_="pager-next")
                    if next_s else t.span(),
                    class_="pager", aria_label=ui["chapters"],
                ),
                class_="doc",
            ),
            t.aside(
                t.p(ui["on_page"], class_="toc-title"),
                t.ul([t.li(t.a(text, href=f"#{hid}")) for hid, text in sec.toc]),
                class_="toc",
            ) if len(sec.toc) > 1 else None,
            class_="wrap docs",
        ), active="docs", description=sec.text[:160], switch_href=twin.href)

    view.__name__ = f"doc_{lang}_{index}"
    return view


def search_index(lang: str):
    def view():
        entries = [{"t": s.title, "u": s.href, "x": s.text[:6000]} for s in SECTIONS[lang]]
        return entries
    view.__name__ = f"search_{lang}"
    return view


for lang in ("en", "fr"):
    p = UI[lang]["prefix"]
    app.add_route(f"{p}/" if p else "/", (lambda l: (lambda: home(l)))(lang), name=f"home_{lang}")
    app.add_route(f"{p}/docs/", (lambda l: (lambda: docs_index(l)))(lang), name=f"docs_{lang}")
    app.add_route(f"{p}/search.json", search_index(lang), name=f"search_{lang}")
    for i in range(len(SECTIONS[lang])):
        app.add_route(f"{p}/docs/{SECTIONS[lang][i].slug}/", doc_page(lang, i), name=f"doc_{lang}_{i}")


# Local development only (python website/app.py): serve exemples/ (playground,
# logo, skills page...) from the site root, as GitHub Pages does after build().
@app.get("/<path:name>")
def exemples_file(name):
    target = (ROOT / "exemples" / name).resolve()
    if not str(target).startswith(str((ROOT / "exemples").resolve())) or not target.is_file():
        abort(404)
    return send_file(str(target))


@app.errorhandler(404)
def not_found():
    return page("en", "404", t.section(t.div(
        t.h1(UI["en"]["notfound"]),
        t.p(t.a(UI["en"]["home"], href=url("/")), " · ", t.a(UI["fr"]["home"], href=url("/fr/"))),
        class_="wrap narrow",
    ), class_="band"))


# ---------------------------------------------------------------------------
# Static export
# ---------------------------------------------------------------------------
# Files of exemples/ published next to the generated pages.
PUBLISHED_EXEMPLES = ["playground.html", "ton618-lite.js", "skills.html", "skills", "logo.svg", "CNAME",
                      "documentation.html"]


def build(out: str) -> None:
    written = app.build(out)
    # The playground, the AI skills page, the logo, the custom-domain CNAME
    # and the legacy single-page documentation.html are served as-is.
    for name in PUBLISHED_EXEMPLES:
        src = ROOT / "exemples" / name
        if src.is_dir():
            shutil.copytree(src, Path(out) / name, dirs_exist_ok=True)
        elif src.exists():
            shutil.copy2(src, Path(out) / name)
    Path(out, ".nojekyll").touch()
    print(f"Built {len(written)} pages into {out}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "build":
        build(sys.argv[2] if len(sys.argv) > 2 else str(HERE / "_site"))
    else:
        app.run()
