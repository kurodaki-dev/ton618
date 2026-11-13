// TON618 documentation site: syntax highlighting (with a TON618 grammar for
// highlight.js), copy / run buttons on code blocks, the docs search, the
// mobile contents menu and the "on this page" highlight.
(function () {
  "use strict";

  // ---- highlight.js grammar for TON618 ------------------------------------
  function ton618(hljs) {
    return {
      name: "TON618",
      aliases: ["ton"],
      keywords: {
        keyword: "if else while for in function return break continue try catch throw finally " +
          "switch case default and or print class extends super self let var const as",
        type: "int float string bool array dict html",
        literal: "true false nil",
        built_in: "len str num format json type assert input push pop shift unshift slice join sort sorted " +
          "reverse contains indexOf map filter reduce find any all keys values has items range sum avg min max " +
          "upper lower trim split replace substring readfile writefile get post serve int fixed isinstance " +
          "zip enumerate unique flatten groupBy forEach count padStart padEnd startsWith endsWith repeat",
      },
      contains: [
        hljs.C_LINE_COMMENT_MODE,
        hljs.C_BLOCK_COMMENT_MODE,
        { className: "meta", begin: /IMPORT:\/\/[\w./]+/ },
        { className: "ton-prefix", begin: /\bton\./ },
        {
          className: "string", begin: /f"/, end: /"/,
          contains: [hljs.BACKSLASH_ESCAPE, { className: "subst", begin: /\{(?!\{)/, end: /\}/ }],
        },
        { className: "string", begin: /"/, end: /"/, contains: [hljs.BACKSLASH_ESCAPE] },
        { className: "string", begin: /`/, end: /`/ },
        { className: "number", begin: /\b(0x[0-9a-fA-F_]+|0b[01_]+|0o[0-7_]+|\d[\d_]*(\.\d+)?([eE][+-]?\d+)?)\b/ },
        { className: "title.function", begin: /\b[A-Za-z_]\w*(?=\s*\()/ },
        { className: "operator", begin: /=>|\|>|\?\.|\?\?=?|\.\.=?|\*\*/ },
      ],
    };
  }

  document.addEventListener("DOMContentLoaded", function () {
    var body = document.body;
    var base = body.dataset.base || "";
    if (window.hljs) {
      window.hljs.registerLanguage("ton", ton618);
      document.querySelectorAll("pre code").forEach(function (el) { window.hljs.highlightElement(el); });
    }

    // ---- Copy and Run buttons ------------------------------------------------
    // "Run" opens the playground with the snippet, unless it needs something a
    // browser can't do (files, network, the HTTP server, user modules...).
    var notInBrowser = /IMPORT:\/\/(?!ton\.(random|time|json|mathutils|strings)\b)|\b(readfile|writefile|serve|get|post|input)\s*\(|^\s*\$|^ton>/m;
    document.querySelectorAll("pre").forEach(function (pre) {
      var code = pre.querySelector("code");
      if (!code) return;
      var tools = document.createElement("div");
      tools.className = "code-tools";
      var copy = document.createElement("button");
      copy.type = "button";
      copy.textContent = body.dataset.copy || "Copy";
      copy.addEventListener("click", function () {
        navigator.clipboard.writeText(code.innerText).then(function () {
          copy.textContent = body.dataset.copied || "Copied!";
          setTimeout(function () { copy.textContent = body.dataset.copy || "Copy"; }, 1400);
        });
      });
      tools.appendChild(copy);
      var isTon = code.classList.contains("language-ton");
      if (isTon && !notInBrowser.test(code.innerText) && /print|=/.test(code.innerText)) {
        var run = document.createElement("a");
        run.className = "run";
        run.textContent = "▶ " + (body.dataset.run || "Run");
        run.href = base + "/playground.html#code=" + encodeURIComponent(code.innerText);
        tools.appendChild(run);
      }
      pre.appendChild(tools);
    });

    // ---- Mobile contents menu --------------------------------------------------
    var sidebar = document.querySelector(".sidebar");
    var toggle = document.querySelector(".sidebar-toggle");
    if (sidebar && toggle) {
      toggle.addEventListener("click", function () {
        var open = sidebar.classList.toggle("open");
        toggle.setAttribute("aria-expanded", String(open));
      });
    }

    // ---- Search --------------------------------------------------------------
    var input = document.querySelector(".search");
    var results = document.querySelector(".search-results");
    var index = null;
    function normalize(s) {
      return s.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "");
    }
    function show(query) {
      var q = normalize(query.trim());
      results.innerHTML = "";
      if (q.length < 2) { results.hidden = true; return; }
      var words = q.split(/\s+/);
      var hits = index.map(function (e) {
        var title = normalize(e.t), text = normalize(e.x);
        var score = 0;
        for (var i = 0; i < words.length; i++) {
          var w = words[i];
          if (title.indexOf(w) >= 0) score += 10;
          var at = text.indexOf(w);
          if (at < 0 && title.indexOf(w) < 0) return null;
          if (at >= 0) score += 1 + Math.min(5, text.split(w).length - 1);
        }
        var pos = text.indexOf(words[0]);
        var snippet = pos >= 0 ? e.x.substr(Math.max(0, pos - 40), 110) : e.x.substr(0, 110);
        return { e: e, score: score, snippet: snippet };
      }).filter(Boolean).sort(function (a, b) { return b.score - a.score; }).slice(0, 8);
      if (!hits.length) {
        var li = document.createElement("li");
        li.className = "empty";
        li.textContent = input.dataset.empty || "No result";
        results.appendChild(li);
      }
      hits.forEach(function (h) {
        var li = document.createElement("li");
        var a = document.createElement("a");
        a.href = h.e.u;
        a.textContent = h.e.t;
        var small = document.createElement("small");
        small.textContent = "…" + h.snippet.trim() + "…";
        a.appendChild(small);
        li.appendChild(a);
        results.appendChild(li);
      });
      results.hidden = false;
    }
    if (input && results) {
      input.addEventListener("input", function () {
        if (index) return show(input.value);
        fetch(input.dataset.index).then(function (r) { return r.json(); }).then(function (data) {
          index = data;
          show(input.value);
        });
      });
      input.addEventListener("keydown", function (ev) {
        if (ev.key === "Enter") {
          var first = results.querySelector("a");
          if (first) location.href = first.href;
        }
        if (ev.key === "Escape") { input.value = ""; results.hidden = true; }
      });
    }
    document.addEventListener("keydown", function (ev) {
      if (ev.key === "/" && input && document.activeElement !== input && !/input|textarea/i.test(document.activeElement.tagName)) {
        ev.preventDefault();
        if (sidebar && !sidebar.classList.contains("open") && toggle && getComputedStyle(toggle).display !== "none") toggle.click();
        input.focus();
      }
    });

    // ---- "On this page" highlight ------------------------------------------------
    var links = Array.prototype.slice.call(document.querySelectorAll(".toc a"));
    if (links.length && "IntersectionObserver" in window) {
      var byId = {};
      links.forEach(function (a) { byId[decodeURIComponent(a.getAttribute("href").slice(1))] = a; });
      var observer = new IntersectionObserver(function (entries) {
        entries.forEach(function (e) {
          if (e.isIntersecting && byId[e.target.id]) {
            links.forEach(function (a) { a.classList.remove("active"); });
            byId[e.target.id].classList.add("active");
          }
        });
      }, { rootMargin: "-80px 0px -70% 0px" });
      document.querySelectorAll(".doc h2[id]").forEach(function (h) { observer.observe(h); });
    }
  });
})();
