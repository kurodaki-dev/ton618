# atome

Build an operating system in a few lines of TON — apps, windows, a terminal, a virtual disk, Windows / Android / custom looks.

```
IMPORT://atome

ton.function hello_init(win) { win["s"]["n"] = 0 }
ton.function hello_view(win) {
    return [atome.title("Hello!"), atome.label("n = " + str(win["s"]["n"])), atome.button("+1", "inc")]
}
ton.function hello_act(win, action, value) { if action == "inc" { win["s"]["n"] += 1 } }
atome.boot("MyOS")
atome.app({"id": "hello", "title": "Hello", "icon": "H", "view": hello_view, "init": hello_init, "act": hello_act})
atome.open("hello")
atome.run()
```

Press Play in Acode (install it from the Modules panel first), the TON618 IDE or the playground, and the system appears.
The `atome` command (`atome new / run / build`) turns the same program into a web page, a kernel, a bootable ISO or an app.
Full guide: https://c-ton618.duckdns.org/ (chapter *Atome*).
