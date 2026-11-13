# stringutils

Small string helpers. It is also the **example package** of the TON618 registry: read its
source (`stringutils.ton`) to see how a module is written.

## Install

```
ton618 install stringutils
```

## Use

```
IMPORT://stringutils

print(stringutils_capitalize("hello"))            // Hello
print(stringutils_titleCase("the black hole"))    // The Black Hole
```

| Function | Does |
|---|---|
| `stringutils_capitalize(s)` | Upper-cases the first letter |
| `stringutils_titleCase(s)` | Upper-cases the first letter of every word |
