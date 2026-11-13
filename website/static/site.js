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

// ---- the registry's database: live package list, package pages and the account page ----
(function () {
  "use strict";
  var body = document.body, api = (body.dataset.api || "").replace(/\/$/, "");
  var fr = body.dataset.lang === "fr";
  if (!api) return;

  function el(tag, props, kids) {
    var e = document.createElement(tag);
    Object.keys(props || {}).forEach(function (k) {
      if (k === "text") e.textContent = props[k];
      else if (k === "class") e.className = props[k];
      else if (k.indexOf("on") === 0) e[k] = props[k];
      else e.setAttribute(k, props[k]);
    });
    (kids || []).forEach(function (c) { if (c) e.append(c); });
    return e;
  }
  function call(method, path, data, key) {
    var headers = {};
    if (data !== undefined) headers["Content-Type"] = "application/json";
    if (key) headers["Authorization"] = "Bearer " + key;
    return fetch(api + path, { method: method, headers: headers, body: data === undefined ? undefined : JSON.stringify(data) })
      .then(function (r) { return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, status: r.status, data: j }; }); });
  }
  function plain(s) { return s.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, ""); }
  function verKey(v) { return v.split(/[.-]/).map(function (x) { return /^\d+$/.test(x) ? ("00000000" + x).slice(-8) : x; }).join("."); }

  document.addEventListener("DOMContentLoaded", function () {
    var list = document.getElementById("pkg-list");
    if (list) livePackages(list);
    var view = document.getElementById("pkg-view");
    if (view) packageView(view);
    var account = document.getElementById("account");
    if (account) accountPage(account);
  });

  // The packages page starts with the packages known when the site was built, then shows the registry's.
  function livePackages(list) {
    fetch(api + "/registry/index.json?t=" + Date.now()).then(function (r) { return r.json(); }).then(function (ix) {
      var known = {};
      [].forEach.call(list.querySelectorAll("li.pkg"), function (li) { known[li.querySelector(".pkg-name").textContent] = li.querySelector(".pkg-name").getAttribute("href"); });
      var names = Object.keys(ix.packages).sort();
      list.textContent = "";
      var base = list.dataset.detail;
      names.forEach(function (n) {
        var p = ix.packages[n];
        var href = known[n] || base + "view/?name=" + encodeURIComponent(n);
        list.append(el("li", { class: "pkg", "data-text": plain(n + " " + p.description + " " + (p.keywords || []).join(" ")) }, [
          el("div", {}, [el("a", { class: "pkg-name", href: href, text: n }), el("span", { class: "pkg-version", text: p.latest })]),
          el("p", { text: p.description }),
          el("code", { text: "ton618 install " + n }),
        ]));
      });
      var count = document.querySelector(".pkg-count");
      if (count) count.textContent = names.length + " " + (names.length === 1 ? (fr ? "paquet" : "package") : (fr ? "paquets" : "packages"));
      var filter = document.getElementById("pkg-filter"), fresh = filter.cloneNode(true);
      filter.replaceWith(fresh);
      fresh.addEventListener("input", function () {
        var q = plain(fresh.value.trim()), shown = 0;
        [].forEach.call(list.children, function (li) { var hit = !q || li.dataset.text.indexOf(q) >= 0; li.hidden = !hit; if (hit) shown++; });
        document.getElementById("pkg-none").hidden = shown > 0;
      });
    }).catch(function () { /* the registry is unreachable: the built list stays */ });
  }

  function packageView(root) {
    var L = JSON.parse(root.dataset.labels), name = new URLSearchParams(location.search).get("name") || "";
    fetch(api + "/registry/index.json?t=" + Date.now()).then(function (r) { return r.json(); }).then(function (ix) {
      var p = ix.packages[name];
      if (!p) { root.append(el("p", { text: L.none || "?" })); return; }
      document.title = name + " · TON618";
      var rows = [[L.latest, p.latest], [L.license, p.license], [L.author, p.owner || p.author]];
      var latest = p.versions[p.latest];
      if (latest && latest.requires) rows.push([L.requires, latest.requires]);
      var deps = latest && latest.dependencies ? Object.keys(latest.dependencies) : [];
      if (deps.length) rows.push([L.needs, deps.map(function (d) { return d + " " + latest.dependencies[d]; }).join(", ")]);
      var versions = Object.keys(p.versions).sort(function (a, b) { return verKey(b) < verKey(a) ? -1 : 1; });
      root.append(
        el("h1", {}, [name + " ", el("span", { class: "pkg-version", text: p.latest })]),
        el("p", { class: "lead", text: p.description }),
        el("pre", {}, [el("code", { text: "ton618 install " + name })]),
        el("table", { class: "facts" }, [el("tbody", {}, rows.filter(function (r) { return r[1]; }).map(function (r) { return el("tr", {}, [el("th", { text: r[0] }), el("td", { text: r[1] })]); }))]),
        el("table", { class: "facts" }, [
          el("thead", {}, [el("tr", {}, [el("th", { text: L.versions }), el("th", { text: "sha256" }), el("th")])]),
          el("tbody", {}, versions.map(function (v) {
            return el("tr", {}, [el("td", {}, [el("code", { text: v })]), el("td", {}, [el("code", { text: p.versions[v].sha256.slice(0, 12) + "…" })]),
              el("td", {}, [el("a", { href: api + "/registry/" + p.versions[v].file, text: L.download })])]);
          })),
        ]),
        el("p", { class: "callout", text: L.caution }),
        el("h2", { text: L.readme }),
        (function () { var box = el("pre", {}, [el("code", { text: "…" })]); readme(name, p.latest, p.versions[p.latest].file, box, L.no_readme); return box; })()
      );
    }).catch(function () { root.append(el("p", { text: "…" })); });
  }
  function readme(name, version, file, box, none) {
    fetch(api + "/registry/" + file).then(function (r) { return r.json(); }).then(function (pkg) {
      var text = pkg.files && Object.keys(pkg.files).filter(function (f) { return /^readme/i.test(f); }).map(function (f) { return pkg.files[f]; })[0];
      box.firstChild.textContent = text || none;
    }).catch(function () { box.firstChild.textContent = none; });
  }

  // ---- account ----
  function accountPage(root) {
    var L = JSON.parse(root.dataset.labels), store = "ton618.session";
    function session() { try { return JSON.parse(localStorage.getItem(store)); } catch (e) { return null; } }
    function save(v) { try { v ? localStorage.setItem(store, JSON.stringify(v)) : localStorage.removeItem(store); } catch (e) { /* private mode */ } }
    var s = session();
    s ? signedIn(s) : signedOut();

    function message(box, text, bad) { box.textContent = text; box.style.color = bad ? "#ff8a8a" : ""; }

    function signedOut() {
      root.textContent = "";
      function form(kind) {
        var out = el("p", { class: "pkg-count", role: "status" });
        var user = el("input", { type: "text", id: kind + "-u", autocomplete: "username", placeholder: L.username, required: "", style: "display:block;margin:0 0 10px" });
        var pass = el("input", { type: "password", id: kind + "-p", autocomplete: kind === "register" ? "new-password" : "current-password", placeholder: L.password, required: "", style: "display:block;margin:0 0 10px" });
        var go = el("button", { type: "submit", text: kind === "register" ? L.register : L.login, style: "font:inherit;padding:8px 14px;cursor:pointer" });
        var f = el("form", { style: "margin:0 0 36px" }, [el("h2", { text: kind === "register" ? L.register : L.login }), user, pass, go, out]);
        f.onsubmit = function (e) {
          e.preventDefault();
          call("POST", "/api/" + kind, { username: user.value, password: pass.value, key_name: "website" }).then(function (r) {
            if (!r.ok) return message(out, r.data.error || L.error, true);
            var v = { key: r.data.api_key, username: r.data.username };
            save(v);
            signedIn(v);
          }).catch(function () { message(out, L.error, true); });
        };
        return f;
      }
      root.append(form("login"), form("register"));
    }

    function signedIn(v) {
      root.textContent = "";
      var fresh = el("div");
      var keysBox = el("div"), pkgBox = el("div");
      var out = el("button", { type: "button", text: L.logout, style: "font:inherit;padding:6px 12px;cursor:pointer;margin-left:12px" });
      out.onclick = function () {
        call("GET", "/api/keys", undefined, v.key).then(function (r) {
          var mine = ((r.data && r.data.keys) || []).filter(function (k) { return k.name === "website"; }).sort(function (a, b) { return b.id - a.id; })[0];
          return mine ? call("DELETE", "/api/keys/" + mine.id, undefined, v.key) : null;
        }).finally(function () { save(null); signedOut(); });
      };
      root.append(el("p", {}, [L.hello + " ", el("b", { text: v.username }), out]), fresh,
        el("h2", { text: L.keys }), keysBox,
        el("h2", { text: L.mine }), pkgBox);

      var name = el("input", { type: "text", placeholder: L.key_name, style: "margin-right:8px" });
      var create = el("button", { type: "button", text: L.create, style: "font:inherit;padding:8px 14px;cursor:pointer" });
      create.onclick = function () {
        call("POST", "/api/keys", { name: name.value || "key" }, v.key).then(function (r) {
          if (!r.ok) return;
          fresh.textContent = "";
          fresh.append(el("p", { class: "callout", text: L.shown_once }),
            el("pre", {}, [el("code", { text: r.data.api_key })]),
            el("p", { text: L.use }), el("pre", {}, [el("code", { text: "ton618 login --token " + r.data.api_key })]));
          name.value = "";
          loadKeys();
        });
      };

      function when(ts) { return ts ? new Date(ts * 1000).toLocaleDateString() : L.never; }
      function loadKeys() {
        call("GET", "/api/keys", undefined, v.key).then(function (r) {
          if (r.status === 401) { save(null); return signedOut(); }
          keysBox.textContent = "";
          ((r.data && r.data.keys) || []).forEach(function (k) {
            var del = el("button", { type: "button", text: L.revoke, style: "font:inherit;padding:3px 9px;cursor:pointer;margin-left:10px" });
            del.onclick = function () { call("DELETE", "/api/keys/" + k.id, undefined, v.key).then(loadKeys); };
            keysBox.append(el("p", { class: "pkg" }, [el("b", { text: k.name }), " · " + L.created + " " + when(k.created_at) + " · " + L.last + " " + when(k.last_used), del]));
          });
          keysBox.append(el("p", {}, [name, create]));
        }).catch(function () { message(keysBox, L.error, true); });
      }
      function loadPackages() {
        call("GET", "/api/me/packages", undefined, v.key).then(function (r) {
          pkgBox.textContent = "";
          var rows = (r.data && r.data.versions) || [];
          if (!rows.length) pkgBox.append(el("p", { text: L.none }));
          rows.forEach(function (p) {
            var item = el("p", { class: "pkg" }, [el("code", { text: p.name + " " + p.version }), p.yanked ? " · " + L.yanked : ""]);
            if (!p.yanked) {
              var hide = el("button", { type: "button", text: L.yank, style: "font:inherit;padding:3px 9px;cursor:pointer;margin-left:10px" });
              hide.onclick = function () { call("POST", "/api/yank/" + p.name + "/" + p.version, {}, v.key).then(loadPackages); };
              item.append(hide);
            }
            pkgBox.append(item);
          });
          pkgBox.append(el("p", { class: "pkg-count", text: L.tip }));
        }).catch(function () { message(pkgBox, L.error, true); });
      }
      loadKeys();
      loadPackages();
    }
  }
})();
