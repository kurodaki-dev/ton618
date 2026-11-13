// TON618 documentation: TON618 syntax colouring, Copy / Run on code examples,
// the search box, and the menu on small screens.
(function () {
  "use strict";
  var body = document.body;
  var root = body.dataset.root || "";
  var fr = body.dataset.lang === "fr";

  // ---- TON618 grammar for highlight.js ---------------------------------------
  function ton(hljs) {
    return {
      name: "TON618",
      keywords: {
        keyword: "if else while for in function return break continue try catch throw finally switch case default " +
          "and or class extends let var const as self super",
        type: "int float string bool array dict html",
        literal: "true false nil",
        built_in: "print len str num format json type range sum min max map filter reduce sorted sort join split " +
          "upper lower trim keys values items has push pop isinstance get post serve readfile writefile",
      },
      contains: [
        hljs.C_LINE_COMMENT_MODE,
        hljs.C_BLOCK_COMMENT_MODE,
        { className: "meta", begin: /IMPORT:\/\/[\w./]+/ },
        { className: "ton", begin: /\bton\./ },
        { className: "string", begin: /f"/, end: /"/, contains: [hljs.BACKSLASH_ESCAPE, { className: "subst", begin: /\{(?!\{)/, end: /\}/ }] },
        { className: "string", begin: /"/, end: /"/, contains: [hljs.BACKSLASH_ESCAPE] },
        { className: "string", begin: /`/, end: /`/ },
        { className: "number", begin: /\b(0x[\da-fA-F_]+|0b[01_]+|0o[0-7_]+|\d[\d_]*(\.\d+)?([eE][+-]?\d+)?)\b/ },
        { className: "title.function", begin: /\b[a-z_]\w*(?=\s*\()/ },
        { className: "title.class", begin: /\b[A-Z]\w*/ },
        { className: "operator", begin: /=>|\|>|\?\.|\?\?=?|\.\.=?|\*\*/ },
      ],
    };
  }

  // Snippets that need a real computer (files, network, server...) get no Run button.
  var NEEDS_COMPUTER = /IMPORT:\/\/(?!ton\.(random|time|json|mathutils|strings)\b)|\b(readfile|writefile|serve|get|post|input)\s*\(|^\s*\$|^ton>/m;

  function decorate(pre) {
    var code = pre.querySelector("code");
    if (!code) return;
    var bar = document.createElement("div");
    bar.className = "actions";
    var copy = document.createElement("button");
    copy.type = "button";
    copy.textContent = fr ? "Copier" : "Copy";
    copy.onclick = function () {
      navigator.clipboard.writeText(code.innerText).then(function () {
        copy.textContent = fr ? "Copié" : "Copied";
        setTimeout(function () { copy.textContent = fr ? "Copier" : "Copy"; }, 1300);
      });
    };
    bar.appendChild(copy);
    if (code.classList.contains("language-ton") && !NEEDS_COMPUTER.test(code.innerText)) {
      var run = document.createElement("a");
      run.textContent = fr ? "Exécuter" : "Run";
      run.href = root + "/playground.html#code=" + encodeURIComponent(code.innerText);
      bar.appendChild(run);
    }
    pre.appendChild(bar);
  }

  // ---- search ------------------------------------------------------------------
  function plain(s) { return s.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, ""); }

  function search(box, list, index) {
    var words = plain(box.value.trim()).split(/\s+/).filter(Boolean);
    list.innerHTML = "";
    if (!words.length || box.value.trim().length < 2) { list.hidden = true; return; }
    var hits = [];
    index.forEach(function (page) {
      var title = plain(page.t), text = plain(page.x), score = 0;
      for (var i = 0; i < words.length; i++) {
        var inTitle = title.indexOf(words[i]) >= 0, at = text.indexOf(words[i]);
        if (!inTitle && at < 0) return;
        score += (inTitle ? 20 : 0) + (at >= 0 ? 1 : 0);
      }
      var where = text.indexOf(words[0]);
      hits.push({ page: page, score: score, around: page.x.substr(Math.max(0, where - 30), 100) });
    });
    hits.sort(function (a, b) { return b.score - a.score; });
    hits.slice(0, 8).forEach(function (h) {
      var li = document.createElement("li"), a = document.createElement("a"), small = document.createElement("small");
      a.href = h.page.u;
      a.textContent = h.page.t;
      small.textContent = h.around.trim() + "…";
      a.appendChild(small);
      li.appendChild(a);
      list.appendChild(li);
    });
    if (!hits.length) {
      var none = document.createElement("li");
      none.className = "none";
      none.textContent = box.dataset.none;
      list.appendChild(none);
    }
    list.hidden = false;
  }

  document.addEventListener("DOMContentLoaded", function () {
    if (window.hljs) {
      window.hljs.registerLanguage("ton", ton);
      document.querySelectorAll("pre code").forEach(function (el) { window.hljs.highlightElement(el); });
    }
    document.querySelectorAll("pre").forEach(decorate);

    var box = document.getElementById("find"), list = document.getElementById("found"), index = null;
    if (box) {
      box.addEventListener("input", function () {
        if (index) return search(box, list, index);
        fetch(box.dataset.source).then(function (r) { return r.json(); }).then(function (data) {
          index = data;
          search(box, list, index);
        });
      });
      box.addEventListener("keydown", function (e) {
        if (e.key === "Enter") { var first = list.querySelector("a"); if (first) location.href = first.href; }
        if (e.key === "Escape") { box.value = ""; list.hidden = true; }
      });
      document.addEventListener("keydown", function (e) {
        var typing = /^(input|textarea|select)$/i.test(document.activeElement.tagName);
        if (e.key === "/" && !typing) { e.preventDefault(); body.classList.add("menu-open"); box.focus(); }
      });
    }

    var filter = document.getElementById("pkg-filter");
    if (filter) {
      var items = [].slice.call(document.querySelectorAll("#pkg-list .pkg"));
      filter.addEventListener("input", function () {
        var q = plain(filter.value.trim()), shown = 0;
        items.forEach(function (li) {
          var hit = !q || plain(li.dataset.text).indexOf(q) >= 0;
          li.hidden = !hit;
          if (hit) shown++;
        });
        document.getElementById("pkg-none").hidden = shown > 0;
      });
    }

    var button = document.getElementById("menu-button");
    if (button) {
      button.addEventListener("click", function (e) {
        e.stopPropagation();
        var open = body.classList.toggle("menu-open");
        button.setAttribute("aria-expanded", String(open));
      });
      document.getElementById("content").addEventListener("click", function () { body.classList.remove("menu-open"); });
    }
  });
})();
