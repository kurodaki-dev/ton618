# requests

HTTP for humans in TON618 — `GET`, `POST`, `PUT`, `PATCH`, `DELETE` over **http and https**, with headers, query parameters, JSON, forms, authentication and timeouts. It needs `curl` on the machine (Windows 10+, macOS and most Linux have it; Termux: `pkg install curl`).

```
ton618 install requests
```

```
IMPORT://requests

ton.let r = requests.get("https://api.github.com/repos/kurodaki-dev/ton618", {"headers": {"Accept": "application/json"}})
if r.succeeded() {
    print(r.json().stargazers_count)
}

ton.let created = requests.post("https://httpbin.org/post", {"json": {"name": "ton"}, "timeout": 10})
print(created.status, created.json().json.name)
```

## Calls

`requests.get(url, [options])`, `.post`, `.put`, `.patch`, `.delete`, `.head`, and `requests.request(method, url, [options])`.

| Option | Meaning |
|---|---|
| `headers` | dict of request headers |
| `params` | dict added to the URL as a query string (encoded for you) |
| `json` | any value, sent as JSON (`Content-Type: application/json`) |
| `data` | a string as is, or a dict sent as a form (`application/x-www-form-urlencoded`) |
| `auth` | `["user", "password"]` — HTTP Basic |
| `bearer` | a token — `Authorization: Bearer ...` |
| `timeout` | seconds (default 30) |
| `verify` | `false` skips the TLS certificate check (default `true`) |

## The response

| | |
|---|---|
| `r.status` | HTTP status code (`0` when no answer arrived) |
| `r.text` | body as text |
| `r.json()` | body parsed as JSON |
| `r.headers` | dict of response headers, names in lower case |
| `r.url` | final URL, after redirects |
| `r.ok` | an answer arrived (even a 404) |
| `r.succeeded()` | `ok` and status 200–299 |
| `r.error` | why no answer arrived (network, TLS, timeout) |
| `r.raise_for_status()` | throws on no answer or 4xx/5xx, returns `r` otherwise |

`requests.quote(text)` and `requests.urlencode(dict)` are the URL-encoding helpers the module uses.

Needs ton618 1.0.0 or newer. It is built on `requests_send`, added to `ton.requests` in 1.0.0.
