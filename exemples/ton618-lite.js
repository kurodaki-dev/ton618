// ============================================================================
// ton618-lite.js — a JavaScript re-implementation of the TON618 language,
// used only by exemples/playground.html to run scripts directly in the
// browser (there's no WebAssembly build of the real C++ interpreter).
//
// beta-2.0.0: classes, the dot operator / uniform call syntax, arrow
// functions, f-strings, ranges, slices, destructuring, ton.let/ton.const,
// |>, ?., ** and the 2.0 standard library are all supported here too —
// tests/*.ton produce the same output in both implementations.
//
// It mirrors the real interpreter's architecture (see DOCUMENTATION.md >
// "Architecture") — Lexer -> Parser -> Interpreter, the same grammar
// (including switch/case), the same native functions — closely enough that
// ordinary scripts behave identically.
// ton.random / ton.time / ton.json / ton.mathutils / ton.strings / ton.encoding
// are re-implemented here in full, since none of those need anything a browser
// can't do. ton.requests runs over a synchronous XMLHttpRequest, so the server
// must allow cross-origin requests (CORS) — that makes the `requests` package work.
//
// What it deliberately leaves out:
//   - ton.sys / ton.os (real process/filesystem access
//     — there's no sandbox equivalent to fall back to)
//   - ton.regex / ton.path / ton.tensor (not reimplemented
//     here yet, though nothing stops them from being — see the real
//     interpreter's registerBuiltinEncoding/Regex/Path/Tensor in
//     src/Interpreter.cpp)
//   - IMPORT://<file> user modules (no filesystem to read them from)
//   - get/post/serve (no HTTP server), readfile/writefile
// Every native/module function is documented in DOCUMENTATION.md; this
// file's job is to run everything that makes sense in a sandbox, and fail
// with a clear, honest message on the rest rather than silently behaving
// differently from the real thing.
// ============================================================================

(function (global) {
"use strict";

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

const KEYWORDS = new Set([
  "if", "else", "while", "for", "in", "function", "return", "break", "continue",
  "try", "catch", "throw", "finally", "switch", "case", "default", "true", "false", "nil", "and", "or", "print",
  "int", "string", "bool", "float", "array", "html", "dict",
]);

function lex(source) {
  const tokens = [];
  let i = 0, line = 1;
  const n = source.length;

  function peek(o = 0) { return source[i + o]; }
  function isDigit(c) { return c >= "0" && c <= "9"; }
  function isAlpha(c) { return c && (/[A-Za-z_]/.test(c)); }
  function isAlnum(c) { return c && (/[A-Za-z0-9_]/.test(c)); }

  function push(type, value) { tokens.push({ type, value, line }); }

  while (i < n) {
    const c = source[i];

    if (c === "\n") { line++; i++; continue; }
    if (c === " " || c === "\t" || c === "\r") { i++; continue; }

    if (c === "/" && peek(1) === "/") { while (i < n && source[i] !== "\n") i++; continue; }
    if (c === "/" && peek(1) === "*") {
      i += 2;
      while (i < n && !(source[i] === "*" && peek(1) === "/")) { if (source[i] === "\n") line++; i++; }
      i += 2;
      continue;
    }

    // "..." and f"..." — mirrors Lexer::string() in src/Lexer.cpp. In an
    // f-string, \{ and \} stay as the {{ / }} escapes the parser understands.
    const isF = c === "f" && peek(1) === '"';
    if (c === '"' || isF) {
      i += isF ? 2 : 1;
      const startLine = line;
      let s = "";
      while (i < n && source[i] !== '"') {
        if (source[i] === "\\") {
          i++;
          const e = source[i++];
          if (e === "u") {
            let hex = "";
            if (source[i] === "{") { i++; while (i < n && source[i] !== "}") hex += source[i++]; i++; }
            else { while (hex.length < 4 && /[0-9a-fA-F]/.test(source[i] || "")) hex += source[i++]; }
            s += String.fromCodePoint(parseInt(hex, 16));
            continue;
          }
          if (e === "{" || e === "}") { s += isF ? e + e : e; continue; }
          s += { n: "\n", t: "\t", r: "\r", '"': '"', "\\": "\\", "0": "\0" }[e] ?? e;
          continue;
        }
        if (source[i] === "\n") line++;
        s += source[i++];
      }
      if (i >= n) throw new Error(`Line ${startLine}: unterminated string.`);
      i++; // closing quote
      tokens.push({ type: isF ? "FSTRING" : "STRING", value: s, line: startLine });
      continue;
    }

    // `raw string` — no escapes, may span lines.
    if (c === "`") {
      i++;
      const startLine = line;
      let s = "";
      while (i < n && source[i] !== "`") { if (source[i] === "\n") line++; s += source[i++]; }
      if (i >= n) throw new Error(`Line ${startLine}: unterminated \`raw string\`.`);
      i++;
      tokens.push({ type: "STRING", value: s, line: startLine });
      continue;
    }

    // 42, 3.14, 1_000, 1e9, 0xFF, 0b1010, 0o755 — mirrors Lexer::number().
    if (isDigit(c)) {
      const kind = c === "0" ? (peek(1) || "").toLowerCase() : "";
      if (kind === "x" || kind === "b" || kind === "o") {
        i += 2;
        let digits = "";
        while (/[0-9a-fA-F_]/.test(source[i] || "")) { if (source[i] !== "_") digits += source[i]; i++; }
        const v = parseInt(digits, kind === "x" ? 16 : kind === "b" ? 2 : 8);
        if (Number.isNaN(v)) throw new Error(`Line ${line}: malformed number literal.`);
        push("NUMBER", v);
        continue;
      }
      let text = "";
      const run = () => { while (isDigit(source[i]) || (source[i] === "_" && isDigit(source[i + 1]))) { if (source[i] !== "_") text += source[i]; i++; } };
      run();
      if (source[i] === "." && isDigit(peek(1))) { text += "."; i++; run(); }
      if ((source[i] === "e" || source[i] === "E") && (isDigit(source[i + 1]) || ((source[i + 1] === "+" || source[i + 1] === "-") && isDigit(source[i + 2])))) {
        text += "e"; i++;
        if (source[i] === "+" || source[i] === "-") text += source[i++];
        run();
      }
      push("NUMBER", parseFloat(text));
      continue;
    }

    if (c === "I" && source.slice(i, i + 9) === "IMPORT://") {
      i += 9;
      push("IMPORT_ARROW");
      continue;
    }

    if (isAlpha(c)) {
      let start = i;
      while (isAlnum(source[i])) i++;
      const text = source.slice(start, i);
      if (text === "ton") push("TON");
      else if (KEYWORDS.has(text)) push(text.toUpperCase());
      else push("IDENTIFIER", text);
      continue;
    }

    if (c === "." && peek(1) === "." && peek(2) === ".") { push("ELLIPSIS"); i += 3; continue; }

    const three = source.slice(i, i + 3);
    const threeMap = { "<<=": "LESS_LESS_EQUAL", ">>=": "GREATER_GREATER_EQUAL", "..=": "DOT_DOT_EQUAL", "**=": "STAR_STAR_EQUAL", "??=": "QUESTION_QUESTION_EQUAL" };
    if (threeMap[three]) { push(threeMap[three]); i += 3; continue; }

    const two = source.slice(i, i + 2);
    const twoMap = {
      "==": "EQUAL_EQUAL", "!=": "BANG_EQUAL", "<=": "LESS_EQUAL", ">=": "GREATER_EQUAL",
      "&&": "AND", "||": "OR", "+=": "PLUS_EQUAL", "-=": "MINUS_EQUAL", "*=": "STAR_EQUAL",
      "/=": "SLASH_EQUAL", "%=": "PERCENT_EQUAL", "++": "PLUS_PLUS", "--": "MINUS_MINUS",
      "??": "QUESTION_QUESTION", "<<": "LESS_LESS", ">>": "GREATER_GREATER",
      "&=": "AMPERSAND_EQUAL", "|=": "PIPE_EQUAL", "^=": "CARET_EQUAL",
      "=>": "ARROW", "..": "DOT_DOT", "|>": "PIPE_GREATER", "**": "STAR_STAR",
    };
    if (twoMap[two]) { push(twoMap[two]); i += 2; continue; }
    // "?." is optional chaining — but not in "c ?.5 : 1" (a ternary with a .5 literal).
    if (two === "?." && !isDigit(peek(2))) { push("QUESTION_DOT"); i += 2; continue; }

    const oneMap = {
      "(": "LPAREN", ")": "RPAREN", "{": "LBRACE", "}": "RBRACE", "[": "LBRACKET", "]": "RBRACKET",
      ",": "COMMA", ";": "SEMICOLON", ".": "DOT", "+": "PLUS", "-": "MINUS", "*": "STAR",
      "/": "SLASH", "%": "PERCENT", "=": "EQUAL", "!": "BANG", "<": "LESS", ">": "GREATER",
      "?": "QUESTION", ":": "COLON", "&": "AMPERSAND", "|": "PIPE", "^": "CARET", "~": "TILDE",
    };
    if (oneMap[c]) { push(oneMap[c]); i++; continue; }

    throw new Error(`Line ${line}: unexpected character '${c}'.`);
  }

  push("EOF");
  return tokens;
}

// ---------------------------------------------------------------------------
// Parser -> a plain-object AST, structurally close to the C++ Expr/Stmt shape
// ---------------------------------------------------------------------------

function parse(tokens) {
  let pos = 0;

  const peek = () => tokens[pos];
  const previous = () => tokens[pos - 1];
  const isAtEnd = () => peek().type === "EOF";
  const check = (t) => (isAtEnd() ? t === "EOF" : peek().type === t);
  const advance = () => { if (!isAtEnd()) pos++; return previous(); };
  const match = (...types) => { for (const t of types) if (check(t)) { advance(); return true; } return false; };
  function consume(type, msg) {
    if (check(type)) return advance();
    const tok = peek();
    throw new Error(`Line ${tok.line} near '${tok.value ?? tok.type}': ${msg}`);
  }

  function parseProgram() {
    const stmts = [];
    while (!isAtEnd()) stmts.push(declaration());
    return stmts;
  }

  function declaration() {
    if (check("TON")) return tonDeclaration();
    if (check("IMPORT_ARROW")) return importDeclaration();
    return statement();
  }

  const TYPE_KEYWORDS = { INT: "int", FLOAT: "float", STRING: "string", BOOL: "bool", ARRAY: "array", HTML: "html", DICT: "dict" };

  function tonDeclaration() {
    advance(); // TON
    consume("DOT", "expected '.' after 'ton'.");
    for (const [tokenType, kind] of Object.entries(TYPE_KEYWORDS)) {
      if (check(tokenType)) { advance(); return varDeclFromType(kind); }
    }
    if (check("FUNCTION")) { advance(); return fnDeclaration(); }
    if (check("IDENTIFIER")) {
      const word = peek().value, next = tokens[pos + 1].type;
      if ((word === "let" || word === "var" || word === "const") && (next === "IDENTIFIER" || next === "LBRACKET" || next === "LBRACE")) {
        advance();
        const d = varDeclFromType("any");
        d.isConst = word === "const";
        return d;
      }
      if (word === "class" && next === "IDENTIFIER") { advance(); return classDeclaration(); }
      if (next === "IDENTIFIER" && tokens[pos + 2] && tokens[pos + 2].type === "EQUAL") {
        const typeName = advance().value;
        const d = varDeclFromType("class");
        d.typeName = typeName;
        return d;
      }
    }
    pos -= 2; // rewind past TON DOT — this is a plain reference used as a statement
    return exprStatement();
  }

  function varDeclFromType(kind) {
    const line = previous().line;
    let name = null, pattern = null;
    if (kind === "any" && (check("LBRACKET") || check("LBRACE"))) pattern = destructuringPattern();
    else name = consume("IDENTIFIER", "expected a variable name after the type.").value;
    consume("EQUAL", "expected '=': a ton.<type> declaration must have an initial value.");
    const init = expression();
    match("SEMICOLON");
    return { kind: "VarDecl", line, name, pattern, init, declaredType: kind };
  }

  // [a, b, ...rest] / {name, age: years, ...rest}
  function destructuringPattern() {
    const isArray = match("LBRACKET");
    if (!isArray) consume("LBRACE", "expected '[' or '{'.");
    const closer = isArray ? "RBRACKET" : "RBRACE";
    const pattern = { isArray, names: [], rest: null };
    if (!check(closer)) {
      do {
        if (check(closer)) break;
        if (match("ELLIPSIS")) { pattern.rest = consume("IDENTIFIER", "expected a name after '...'.").value; break; }
        if (check("TON")) { advance(); consume("DOT", "expected '.' after 'ton'."); }
        const first = consume("IDENTIFIER", "expected a variable name in the pattern.").value;
        if (!isArray && match("COLON")) pattern.names.push([first, consume("IDENTIFIER", "expected a variable name after ':'.").value]);
        else pattern.names.push([isArray ? null : first, first]);
      } while (match("COMMA"));
    }
    consume(closer, isArray ? "expected ']' to close the pattern." : "expected '}' to close the pattern.");
    return pattern;
  }

  function classDeclaration() {
    const line = previous().line;
    const name = consume("IDENTIFIER", "expected a class name.").value;
    let superName = null;
    if (check("IDENTIFIER") && peek().value === "extends") {
      advance();
      if (check("TON")) { advance(); consume("DOT", "expected '.' after 'ton'."); }
      superName = consume("IDENTIFIER", "expected a parent class name after 'extends'.").value;
    }
    consume("LBRACE", "expected '{' to start the class body.");
    const members = [];
    while (!check("RBRACE") && !isAtEnd()) {
      if (match("SEMICOLON")) continue;
      if (!check("TON")) throw new Error(`Line ${peek().line}: a class body can only contain 'ton.function' methods and 'ton.<type>' fields.`);
      const m = tonDeclaration();
      if (m.kind !== "FnDecl" && m.kind !== "VarDecl") throw new Error(`Line ${m.line}: a class body can only contain 'ton.function' methods and 'ton.<type>' fields.`);
      members.push(m);
    }
    consume("RBRACE", "expected '}' at the end of the class body.");
    return { kind: "ClassDecl", line, name, superName, members };
  }

  // "ton.function name(...) { ... }" as a top-level statement — always a
  // named declaration, bound as a variable in the enclosing scope.
  function fnDeclaration() {
    const line = previous().line;
    const name = consume("IDENTIFIER", "expected a function name.").value;
    const { params, paramDefaults, hasRest, body } = functionRest();
    return { kind: "FnDecl", line, name, params, paramDefaults, hasRest, body };
  }

  // Parses "(params) { body }", shared by both the statement form above and
  // the expression form in tonReference() below.
  function functionRest() {
    consume("LPAREN", "expected '(' after the function name.");
    const { names: params, defaults: paramDefaults, hasRest } = parseParamList();
    consume("RPAREN", "expected ')' after the parameters.");
    consume("LBRACE", "expected '{' before the function body.");
    return { params, paramDefaults, hasRest, body: block() };
  }

  // Parses "(a, b = default, ...rest)" — "..." may only appear on the last
  // parameter.
  function parseParamList() {
    const names = [];
    const defaults = [];
    let hasRest = false;
    if (!check("RPAREN")) {
      do {
        if (check("TON")) { advance(); consume("DOT", "expected '.' after 'ton'."); }
        if (match("ELLIPSIS")) {
          if (check("TON")) { advance(); consume("DOT", "expected '.' after 'ton'."); }
          names.push(consume("IDENTIFIER", "expected a parameter name after '...'.").value);
          defaults.push(null);
          hasRest = true;
          break;
        }
        names.push(consume("IDENTIFIER", "expected a parameter name.").value);
        defaults.push(match("EQUAL") ? expression() : null);
      } while (match("COMMA"));
    }
    return { names, defaults, hasRest };
  }

  function importDeclaration() {
    const line = peek().line;
    advance(); // IMPORT_ARROW
    let moduleName;
    if (check("TON")) {
      advance();
      consume("DOT", "expected '.' after 'ton'.");
      moduleName = "ton." + consume("IDENTIFIER", "expected a built-in module name after 'ton.'.").value;
    } else {
      moduleName = consume("IDENTIFIER", "expected a module name after 'IMPORT://'.").value;
      while (check("SLASH") && tokens[pos + 1].type === "IDENTIFIER") { advance(); moduleName += "/" + advance().value; }
    }
    let alias = null;
    if (check("IDENTIFIER") && peek().value === "as" && tokens[pos + 1].type === "IDENTIFIER") { advance(); alias = advance().value; }
    match("SEMICOLON");
    return { kind: "Import", line, moduleName, alias };
  }

  function statement() {
    if (match("IF")) return ifStatement();
    if (match("WHILE")) return whileStatement();
    if (match("FOR")) return forStatement();
    if (match("TRY")) return tryStatement();
    if (match("THROW")) return throwStatement();
    if (match("SWITCH")) return switchStatement();
    if (match("RETURN")) return returnStatement();
    if (check("PRINT") && tokens[pos + 1].type === "LPAREN") { advance(); return printStatement(); }
    if (match("LBRACE")) return block();
    if (match("BREAK")) { match("SEMICOLON"); return { kind: "Break" }; }
    if (match("CONTINUE")) { match("SEMICOLON"); return { kind: "Continue" }; }
    return exprStatement();
  }

  function ifStatement() {
    const line = previous().line;
    const paren = match("LPAREN");
    const cond = expression();
    if (paren) consume("RPAREN", "expected ')' after the condition.");
    consume("LBRACE", "expected '{' for the if block.");
    const thenBranch = block();
    let elseBranch = null;
    if (match("ELSE")) {
      elseBranch = match("IF") ? ifStatement() : (consume("LBRACE", "expected '{' for the else block."), block());
    }
    return { kind: "If", line, cond, thenBranch, elseBranch };
  }

  function whileStatement() {
    const line = previous().line;
    const paren = match("LPAREN");
    const cond = expression();
    if (paren) consume("RPAREN", "expected ')' after the condition.");
    consume("LBRACE", "expected '{' for the loop body.");
    const body = block();
    return { kind: "While", line, cond, body };
  }

  function forStatement() {
    const line = previous().line;
    const paren = match("LPAREN");

    {
      const saved = pos;
      const loopName = () => {
        if (check("TON") && tokens[pos + 1].type === "DOT") { advance(); advance(); }
        return check("IDENTIFIER") ? advance().value : null;
      };
      const first = loopName();
      let second = null, ok = first !== null;
      if (ok && match("COMMA")) { second = loopName(); ok = second !== null; }
      if (ok && match("IN")) {
        const iterable = expression();
        if (paren) consume("RPAREN", "expected ')' after the for-in collection.");
        consume("LBRACE", "expected '{' for the for-in body.");
        const body = block();
        return { kind: "ForIn", line, name: first, name2: second, iterable, body };
      }
      pos = saved;
    }

    let init = null;
    if (match("SEMICOLON")) init = null;
    else if (check("TON")) init = tonDeclaration();
    else init = exprStatement();

    let cond = null;
    if (!check("SEMICOLON")) cond = expression();
    consume("SEMICOLON", "expected ';' after the for condition.");

    let incr = null;
    if (paren) {
      if (!check("RPAREN")) incr = expression();
      consume("RPAREN", "expected ')' after the for clause.");
    } else if (!check("LBRACE")) {
      incr = expression();
    }

    consume("LBRACE", "expected '{' for the for body.");
    const body = block();
    return { kind: "For", line, init, cond, incr, body };
  }

  function tryStatement() {
    const line = previous().line;
    consume("LBRACE", "expected '{' after 'try'.");
    const tryBlock = block();
    let errorVar = null;
    let catchBlock = null;
    if (match("CATCH")) {
      const cparen = match("LPAREN");
      if (check("TON")) { advance(); consume("DOT", "expected '.' after 'ton'."); }
      errorVar = consume("IDENTIFIER", "expected an error variable name.").value;
      if (cparen) consume("RPAREN", "expected ')' after the catch variable.");
      consume("LBRACE", "expected '{' for the catch block.");
      catchBlock = block();
    }
    let finallyBlock = null;
    if (match("FINALLY")) {
      consume("LBRACE", "expected '{' after 'finally'.");
      finallyBlock = block();
    }
    if (!catchBlock && !finallyBlock) {
      const tok = previous();
      throw new Error(`Line ${tok.line} near '${tok.value ?? tok.type}': expected 'catch' or 'finally' after the try block.`);
    }
    return { kind: "TryCatch", line, tryBlock, errorVar, catchBlock, finallyBlock };
  }

  // "switch (subject) { case v1, v2: { ... } case v3: { ... } default: { ... } }"
  // — no fallthrough, mirrors the real interpreter's Parser::switchStatement.
  function switchStatement() {
    const line = previous().line;
    const paren = match("LPAREN");
    const subject = expression();
    if (paren) consume("RPAREN", "expected ')' after the switch value.");
    consume("LBRACE", "expected '{' to start the switch body.");

    const cases = [];
    let defaultBlock = null;
    while (!check("RBRACE") && !isAtEnd()) {
      if (match("CASE")) {
        const values = [expression()];
        while (match("COMMA")) values.push(expression());
        consume("COLON", "expected ':' after the case value(s).");
        consume("LBRACE", "expected '{' for the case body.");
        cases.push({ values, body: block() });
      } else if (match("DEFAULT")) {
        consume("COLON", "expected ':' after 'default'.");
        consume("LBRACE", "expected '{' for the default body.");
        defaultBlock = block();
      } else {
        const tok = peek();
        throw new Error(`Line ${tok.line} near '${tok.value ?? tok.type}': expected 'case' or 'default' inside a switch body.`);
      }
    }
    consume("RBRACE", "expected '}' at the end of the switch.");
    return { kind: "Switch", line, subject, cases, defaultBlock };
  }

  function throwStatement() {
    const line = previous().line;
    const value = expression();
    match("SEMICOLON");
    return { kind: "Throw", line, value };
  }

  function returnStatement() {
    const line = previous().line;
    let value = null;
    if (!check("SEMICOLON") && !check("RBRACE")) value = expression();
    match("SEMICOLON");
    return { kind: "Return", line, value };
  }

  function printStatement() {
    const line = previous().line;
    consume("LPAREN", "expected '(' after 'print'.");
    const parts = [];
    if (!check("RPAREN")) { do { if (check("RPAREN")) break; parts.push(expression()); } while (match("COMMA")); }
    consume("RPAREN", "expected ')' after the print arguments.");
    match("SEMICOLON");
    // print(a, b, c) prints the values separated by spaces.
    const value = parts.length === 1 ? parts[0]
      : { kind: "FString", line, parts: parts.flatMap((p, i) => (i ? [{ kind: "Literal", value: " " }, p] : [p])) };
    return { kind: "Print", line, value };
  }

  function block() {
    const stmts = [];
    while (!check("RBRACE") && !isAtEnd()) stmts.push(declaration());
    consume("RBRACE", "expected '}' at the end of the block.");
    return { kind: "Block", statements: stmts };
  }

  function exprStatement() {
    const line = peek().line;
    const e = expression();
    match("SEMICOLON");
    return { kind: "ExprStmt", line, expr: e };
  }

  // ---- expressions ----

  function expression() { return assignment(); }

  function makeAssignTarget(target, value, line) {
    if (target.kind === "Variable") return { kind: "Assign", line, name: target.name, value };
    if (target.kind === "Index") return { kind: "Index", line, target: target.target, index: target.index, value };
    if (target.kind === "Get") {
      if (target.optional) throw new Error(`Line ${line}: cannot assign through '?.'.`);
      return { kind: "Get", line, object: target.object, name: target.name, value };
    }
    throw new Error(`Line ${line}: invalid assignment target.`);
  }

  const COMPOUND_OPS = {
    PLUS_EQUAL: "PLUS", MINUS_EQUAL: "MINUS", STAR_EQUAL: "STAR", SLASH_EQUAL: "SLASH", PERCENT_EQUAL: "PERCENT",
    AMPERSAND_EQUAL: "AMPERSAND", PIPE_EQUAL: "PIPE", CARET_EQUAL: "CARET",
    LESS_LESS_EQUAL: "LESS_LESS", GREATER_GREATER_EQUAL: "GREATER_GREATER", STAR_STAR_EQUAL: "STAR_STAR",
  };

  function assignment() {
    const expr = pipe();
    if (match("QUESTION_QUESTION_EQUAL")) {
      const line = previous().line;
      return makeAssignTarget(expr, { kind: "Logical", line, op: "??", left: expr, right: assignment() }, line);
    }
    if (match("EQUAL")) {
      const line = previous().line;
      return makeAssignTarget(expr, assignment(), line);
    }
    for (const [tokType, op] of Object.entries(COMPOUND_OPS)) {
      if (check(tokType)) {
        const line = advance().line;
        const rhs = assignment();
        const binary = { kind: "Binary", line, left: expr, op, right: rhs };
        return makeAssignTarget(expr, binary, line);
      }
    }
    return expr;
  }

  // x |> f(a)  ->  f(x, a)
  function pipe() {
    let expr = ternary();
    while (match("PIPE_GREATER")) {
      const line = previous().line;
      const right = ternary();
      if (right.kind === "Call") { right.args.unshift(expr); expr = right; }
      else expr = { kind: "Call", line, callee: right, args: [expr] };
    }
    return expr;
  }

  function ternary() {
    const expr = nilCoalesce();
    if (match("QUESTION")) {
      const line = previous().line;
      const whenTrue = expression();
      consume("COLON", "expected ':' in the ternary expression.");
      const whenFalse = ternary();
      return { kind: "Ternary", line, cond: expr, whenTrue, whenFalse };
    }
    return expr;
  }

  function nilCoalesce() {
    let expr = logicOr();
    while (match("QUESTION_QUESTION")) {
      const line = previous().line;
      expr = { kind: "Logical", line, op: "??", left: expr, right: logicOr() };
    }
    return expr;
  }

  function binaryLevel(next, ops) {
    return function () {
      let expr = next();
      while (ops.includes(peek().type)) {
        const opTok = advance();
        const right = next();
        expr = { kind: "Binary", line: opTok.line, left: expr, op: opTok.type, right };
      }
      return expr;
    };
  }

  function logicOr() {
    let expr = logicAnd();
    while (match("OR")) { const line = previous().line; expr = { kind: "Logical", line, op: "or", left: expr, right: logicAnd() }; }
    return expr;
  }
  function logicAnd() {
    let expr = bitwise();
    while (match("AND")) { const line = previous().line; expr = { kind: "Logical", line, op: "and", left: expr, right: bitwise() }; }
    return expr;
  }
  // "a & b", "a | b", "a ^ b" — one precedence level (left to right), between
  // logicAnd and equality, mirroring src/Parser.cpp's bitwise().
  const bitwise = binaryLevel(() => equality(), ["AMPERSAND", "PIPE", "CARET"]);
  const equality = binaryLevel(() => comparison(), ["EQUAL_EQUAL", "BANG_EQUAL"]);
  const comparison = binaryLevel(() => range(), ["LESS", "LESS_EQUAL", "GREATER", "GREATER_EQUAL", "IN"]);
  function range() {
    const expr = shift();
    if (match("DOT_DOT", "DOT_DOT_EQUAL")) {
      const tok = previous();
      return { kind: "Range", line: tok.line, left: expr, right: shift(), inclusive: tok.type === "DOT_DOT_EQUAL" };
    }
    return expr;
  }
  const shift = binaryLevel(() => term(), ["LESS_LESS", "GREATER_GREATER"]);
  const term = binaryLevel(() => factor(), ["PLUS", "MINUS"]);
  const factor = binaryLevel(() => unary(), ["STAR", "SLASH", "PERCENT"]);

  function unary() {
    if (match("BANG", "MINUS", "TILDE")) {
      const opTok = previous();
      return { kind: "Unary", line: opTok.line, op: opTok.type, operand: unary() };
    }
    return power();
  }

  function power() {
    const expr = call();
    if (match("STAR_STAR")) return { kind: "Binary", line: previous().line, left: expr, op: "STAR_STAR", right: unary() };
    return expr;
  }

  function finishArgs() {
    const args = [];
    if (!check("RPAREN")) {
      do {
        if (check("RPAREN")) break;
        if (match("ELLIPSIS")) args.push({ kind: "Spread", line: previous().line, expr: expression() });
        else args.push(expression());
      } while (match("COMMA"));
    }
    consume("RPAREN", "expected ')' after the arguments.");
    return args;
  }

  function finishGet(object, optional) {
    const t = peek();
    const name = t.value !== undefined && typeof t.value === "string" ? t.value : t.type.toLowerCase();
    if (t.type === "EOF" || !/^[A-Za-z_]/.test(name)) throw new Error(`Line ${t.line}: expected a property name after '.'.`);
    advance();
    return { kind: "Get", line: t.line, object, name, optional };
  }

  function finishIndex(target, optional) {
    const line = previous().line;
    let first = null;
    if (!check("COLON")) first = expression();
    if (match("COLON")) {
      const end = check("RBRACKET") ? null : expression();
      consume("RBRACKET", "expected ']' after the slice.");
      return { kind: "Slice", line, target, start: first, end, optional };
    }
    consume("RBRACKET", "expected ']' after the index.");
    return { kind: "Index", line, target, index: first, optional };
  }

  function call() {
    let expr = primary();
    while (true) {
      if (match("LPAREN")) {
        const line = previous().line;
        expr = { kind: "Call", line, callee: expr, args: finishArgs() };
      } else if (match("LBRACKET")) {
        expr = finishIndex(expr, false);
      } else if (match("DOT")) {
        expr = finishGet(expr, false);
      } else if (match("QUESTION_DOT")) {
        if (match("LBRACKET")) expr = finishIndex(expr, true);
        else if (match("LPAREN")) expr = { kind: "Call", line: previous().line, callee: expr, args: finishArgs(), optional: true };
        else expr = finishGet(expr, true);
      } else if (check("PLUS_PLUS") || check("MINUS_MINUS")) {
        const opTok = advance();
        const op = opTok.type === "PLUS_PLUS" ? "PLUS" : "MINUS";
        const one = { kind: "Literal", value: 1 };
        const binary = { kind: "Binary", line: opTok.line, left: expr, op, right: one };
        expr = makeAssignTarget(expr, binary, opTok.line);
      } else {
        break;
      }
    }
    return expr;
  }

  // Parses "ton.identifier" as an expression (a variable read, or a call's
  // callee), or "ton.function [name](...) { ... }" as an inline function
  // value — the optional name is only a debug label here, never bound as a
  // variable the way a top-level "ton.function name(...) {}" declaration is.
  function tonReference() {
    const tonTok = advance(); // TON
    consume("DOT", "expected '.' after 'ton'.");
    if (check("FUNCTION")) {
      advance();
      const name = check("IDENTIFIER") ? advance().value : null;
      const { params, paramDefaults, hasRest, body } = functionRest();
      return { kind: "FnExpr", line: tonTok.line, name, params, paramDefaults, hasRest, body };
    }
    const name = consume("IDENTIFIER", "expected a name after 'ton.'.").value;
    return { kind: "Variable", line: tonTok.line, name };
  }

  function primary() {
    if (match("FALSE")) return { kind: "Literal", value: false };
    if (match("TRUE")) return { kind: "Literal", value: true };
    if (match("NIL")) return { kind: "Literal", value: null };
    if (match("NUMBER")) return { kind: "Literal", value: previous().value };
    if (match("STRING")) return { kind: "Literal", value: previous().value };
    if (match("FSTRING")) return fstring(previous());
    if (match("PRINT")) return { kind: "Variable", line: previous().line, name: "print" };
    if (["INT", "FLOAT", "BOOL", "DICT", "STRING", "ARRAY"].includes(peek().type) && tokens[pos + 1].type === "LPAREN") {
      const t = advance();
      return { kind: "Variable", line: t.line, name: t.type.toLowerCase() };
    }
    if (check("IDENTIFIER") && tokens[pos + 1].type === "ARROW") {
      const p = advance(); advance();
      return arrowBody({ names: [p.value], defaults: [null], hasRest: false }, p.line);
    }
    if (check("LPAREN") && isArrowAhead()) {
      const line = advance().line;
      const params = parseParamList();
      consume("RPAREN", "expected ')' after the parameters.");
      consume("ARROW", "expected '=>'.");
      return arrowBody(params, line);
    }
    if (check("IDENTIFIER") && peek().value === "super" && tokens[pos + 1].type === "DOT") {
      const line = advance().line; advance();
      return { kind: "Super", line, name: consume("IDENTIFIER", "expected a method name after 'super.'.").value };
    }
    if (check("TON")) return tonReference();
    if (match("IDENTIFIER")) return { kind: "Variable", line: previous().line, name: previous().value };
    if (match("LPAREN")) {
      const inner = expression();
      consume("RPAREN", "expected ')' after the expression.");
      return inner;
    }
    if (match("LBRACKET")) {
      const elements = [];
      if (!check("RBRACKET")) {
        do {
          if (match("ELLIPSIS")) elements.push({ kind: "Spread", line: previous().line, expr: expression() });
          else elements.push(expression());
        } while (match("COMMA"));
      }
      consume("RBRACKET", "expected ']' after the array elements.");
      return { kind: "ArrayLit", elements };
    }
    if (match("LBRACE")) {
      const entries = [];
      if (!check("RBRACE")) {
        do {
          if (match("ELLIPSIS")) {
            entries.push([null, { kind: "Spread", line: previous().line, expr: expression() }]);
            continue;
          }
          let key;
          if (check("STRING")) key = advance().value;
          else key = consume("IDENTIFIER", "expected a key name in the dict literal.").value;
          consume("COLON", "expected ':' after the dict key.");
          entries.push([key, expression()]);
        } while (match("COMMA"));
      }
      consume("RBRACE", "expected '}' after the dict literal.");
      return { kind: "DictLit", entries };
    }
    const tok = peek();
    throw new Error(`Line ${tok.line}: expected an expression.`);
  }

  function isArrowAhead() {
    let depth = 0;
    for (let i = pos; i < tokens.length; i++) {
      const t = tokens[i].type;
      if (t === "LPAREN") depth++;
      else if (t === "RPAREN") { if (--depth === 0) return tokens[i + 1] && tokens[i + 1].type === "ARROW"; }
      else if (t === "EOF") return false;
    }
    return false;
  }

  function arrowBody(params, line) {
    let body;
    if (match("LBRACE")) body = block();
    else body = { kind: "Block", statements: [{ kind: "Return", line: peek().line, value: expression() }] };
    return { kind: "FnExpr", line, name: "<arrow>", params: params.names, paramDefaults: params.defaults, hasRest: params.hasRest, body };
  }

  // f"Hello {name}" -> alternating literal / expression parts.
  function fstring(tok) {
    const t = tok.value, parts = [];
    let lit = "";
    const flush = () => { if (lit) { parts.push({ kind: "Literal", value: lit }); lit = ""; } };
    for (let i = 0; i < t.length; i++) {
      const c = t[i];
      if (c === "{" && t[i + 1] === "{") { lit += "{"; i++; continue; }
      if (c === "}" && t[i + 1] === "}") { lit += "}"; i++; continue; }
      if (c === "}") throw new Error(`Line ${tok.line}: single '}' in an f-string (write '}}' for a literal brace).`);
      if (c !== "{") { lit += c; continue; }
      let j = i + 1, depth = 1, quote = null;
      for (; j < t.length; j++) {
        const d = t[j];
        if (quote) { if (d === "\\") j++; else if (d === quote) quote = null; continue; }
        if (d === '"' || d === "`") quote = d;
        else if (d === "{") depth++;
        else if (d === "}" && --depth === 0) break;
      }
      if (j >= t.length) throw new Error(`Line ${tok.line}: unclosed '{' in an f-string.`);
      const src = t.slice(i + 1, j);
      if (!src.trim()) throw new Error(`Line ${tok.line}: empty '{}' in an f-string.`);
      flush();
      const sub = lex(src).map((x) => ({ ...x, line: tok.line }));
      parts.push(parseExpressionOnly(sub, tok.line, src));
      i = j;
    }
    flush();
    return { kind: "FString", line: tok.line, parts };
  }

  if (tokens.__expressionOnly) {
    const e = expression();
    if (!isAtEnd()) throw new Error(`Line ${tokens.__line}: invalid expression '{${tokens.__src}}' in an f-string.`);
    return e;
  }
  return parseProgram();
}

function parseExpressionOnly(tokens, line, src) {
  tokens.__expressionOnly = true;
  tokens.__line = line;
  tokens.__src = src;
  return parse(tokens);
}

// ---------------------------------------------------------------------------
// Values — plain JS values double as TON618 values, with two wrapper classes
// for the cases JS has no native equivalent for: HTML-tagged strings and dicts
// (kept as insertion-ordered Maps, matching the real interpreter's semantics).
// ---------------------------------------------------------------------------

class HtmlString { constructor(s) { this.value = s; } }
class TonFunction { constructor(name, params, body, closure, paramDefaults = [], hasRest = false) { Object.assign(this, { name, params, body, closure, paramDefaults, hasRest }); } }
class NativeFunction { constructor(name, fn) { this.name = name; this.fn = fn; } }
// beta-2.0.0: user-defined classes and their instances (see ClassObj /
// InstanceObj in include/Value.hpp). Fields are an insertion-ordered Map.
class TonClass {
  constructor(name, superclass, methods, fieldInits, closure) { Object.assign(this, { name, superclass, methods, fieldInits, closure }); }
  findMethod(m) { for (let c = this; c; c = c.superclass) if (c.methods.has(m)) return c.methods.get(m); return null; }
  isSubclassOf(o) { for (let c = this; c; c = c.superclass) if (c === o) return true; return false; }
}
class TonInstance { constructor(klass) { this.klass = klass; this.fields = new Map(); } }
// "throw value" keeps the value itself (catch receives exactly what was thrown).
class ThrownValue extends Error { constructor(value) { super(toDisplayString(value)); this.value = value; } }
let instanceToString = null; // set by the Interpreter: uses a class's toString() method

function repr(v) { return (typeof v === "string" || v instanceof HtmlString) ? JSON.stringify(toDisplayString(v)) : toDisplayString(v); }
function formatNumber(v) {
  if (Number.isNaN(v)) return "nan";
  if (!Number.isFinite(v)) return v > 0 ? "inf" : "-inf";
  return String(v);
}

function typeName(v) {
  if (v === null || v === undefined) return "nil";
  if (typeof v === "number") return "int/float";
  if (typeof v === "string") return "string";
  if (typeof v === "boolean") return "bool";
  if (Array.isArray(v)) return "array";
  if (v instanceof Map) return "dict";
  if (v instanceof HtmlString) return "html";
  if (v instanceof TonFunction || v instanceof NativeFunction) return "function";
  if (v instanceof TonClass) return "class";
  if (v instanceof TonInstance) return v.klass.name;
  return "unknown";
}

function isTruthy(v) {
  if (v === null || v === undefined) return false;
  if (typeof v === "boolean") return v;
  if (typeof v === "number") return v !== 0;
  if (typeof v === "string") return v.length > 0;
  if (v instanceof HtmlString) return v.value.length > 0;
  return true;
}

function toDisplayString(v) {
  if (v === null || v === undefined) return "nil";
  if (typeof v === "boolean") return v ? "true" : "false";
  if (typeof v === "number") return formatNumber(v);
  if (typeof v === "string") return v;
  if (v instanceof HtmlString) return v.value;
  if (Array.isArray(v)) return "[" + v.map(toDisplayString).join(", ") + "]";
  if (v instanceof Map) return "{" + [...v.entries()].map(([k, val]) => `${k}: ${toDisplayString(val)}`).join(", ") + "}";
  if (v instanceof TonFunction) return `<ton.function ${v.name || ""}>`;
  if (v instanceof NativeFunction) return "<native function>";
  if (v instanceof TonClass) return `<ton.class ${v.name}>`;
  if (v instanceof TonInstance) {
    if (instanceToString) { const r = instanceToString(v); if (r !== undefined) return r; }
    return v.klass.name + "{" + [...v.fields.entries()].map(([k, val]) => `${k}: ${repr(val)}`).join(", ") + "}";
  }
  return String(v);
}

function toJson(v) {
  if (v === null || v === undefined) return "null";
  if (typeof v === "boolean") return v ? "true" : "false";
  if (typeof v === "number") return String(v);
  if (typeof v === "string" || v instanceof HtmlString) {
    const s = v instanceof HtmlString ? v.value : v;
    return JSON.stringify(s);
  }
  if (Array.isArray(v)) return "[" + v.map(toJson).join(",") + "]";
  if (v instanceof Map) return "{" + [...v.entries()].map(([k, val]) => JSON.stringify(k) + ":" + toJson(val)).join(",") + "}";
  if (v instanceof TonInstance) return toJson(v.fields);
  return "null";
}

// Same shape as toJson(), but indented across multiple lines — mirrors
// Value::toJsonPretty() in include/Value.hpp.
function toJsonPretty(v, indent = 0) {
  const pad = "  ".repeat(indent), padInner = "  ".repeat(indent + 1);
  if (Array.isArray(v)) {
    if (!v.length) return "[]";
    const items = v.map((item) => padInner + toJsonPretty(item, indent + 1));
    return "[\n" + items.join(",\n") + "\n" + pad + "]";
  }
  if (v instanceof Map) {
    if (!v.size) return "{}";
    const items = [...v.entries()].map(([k, val]) => padInner + JSON.stringify(k) + ": " + toJsonPretty(val, indent + 1));
    return "{\n" + items.join(",\n") + "\n" + pad + "}";
  }
  return toJson(v);
}

function fromJson(text) { return jsToTon(JSON.parse(text)); }
function jsToTon(v) {
  if (v === null) return null;
  if (Array.isArray(v)) return v.map(jsToTon);
  if (typeof v === "object") { const m = new Map(); for (const k of Object.keys(v)) m.set(k, jsToTon(v[k])); return m; }
  return v;
}

// ---------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------

class Environment {
  constructor(parent = null) { this.values = new Map(); this.parent = parent; this.consts = null; }
  define(name, v, isConst = false) {
    this.values.set(name, v);
    if (isConst) (this.consts ||= new Set()).add(name);
    else if (this.consts) this.consts.delete(name);
  }
  get(name) {
    if (this.values.has(name)) return this.values.get(name);
    if (this.parent) return this.parent.get(name);
    throw new Error(`Undefined variable or function 'ton.${name}'.${suggest(name, this)}`);
  }
  has(name) { return this.values.has(name) || (this.parent && this.parent.has(name)); }
  // true, false (undeclared) or "const"
  assign(name, v) {
    if (this.values.has(name)) {
      if (this.consts && this.consts.has(name)) return "const";
      this.values.set(name, v);
      return true;
    }
    if (this.parent) return this.parent.assign(name, v);
    return false;
  }
}

function levenshtein(a, b) {
  let prev = Array.from({ length: b.length + 1 }, (_, j) => j);
  for (let i = 1; i <= a.length; i++) {
    const cur = [i];
    for (let j = 1; j <= b.length; j++) cur[j] = Math.min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
    prev = cur;
  }
  return prev[b.length];
}
function suggest(name, env) {
  let best = null, bestDist = name.length <= 3 ? 1 : 2;
  for (let e = env; e; e = e.parent) {
    for (const cand of e.values.keys()) {
      const d = levenshtein(name, cand);
      if (d <= bestDist && (best === null || d < bestDist)) { best = cand; bestDist = d; }
    }
  }
  return best ? ` Did you mean '${best}'?` : "";
}

class BreakSignal {}
class ContinueSignal {}
class ReturnSignal { constructor(value) { this.value = value; } }

// ---------------------------------------------------------------------------
// Interpreter
// ---------------------------------------------------------------------------

// <atome-module> generated by scripts/embed_atome.py from atome/module/atome.ton: edit that file, not this block
const ATOME_MODULE_SOURCE = "// atome.ton — build an operating system in a few lines of TON.\n//\n//   IMPORT://atome\n//   ton.function hello_view(win) { return [atome.label(\"Hello!\"), atome.button(\"+1\", \"inc\"), atome.label(\"n = \" + str(win[\"s\"][\"n\"]))] }\n//   ton.function hello_init(win) { win[\"s\"][\"n\"] = 0 }\n//   ton.function hello_act(win, action, value) { if action == \"inc\" { win[\"s\"][\"n\"] += 1 } }\n//   atome.boot(\"MyOS\")\n//   atome.app({\"id\": \"hello\", \"title\": \"Hello\", \"icon\": \"H\", \"view\": hello_view, \"init\": hello_init, \"act\": hello_act})\n//   atome.run()\n//\n// One program, two places to run it: press Play in Acode, the TON618 IDE or the playground and the system\n// appears as a graphical desktop (windows you drag, a taskbar, or a phone screen); build it with `atome build`\n// and the same program boots on the Atome kernel as a text-mode system (arrows, Enter, Tab, Esc).\n//\n// This file only uses the part of TON that the kernel understands. Everything is in the `atome` dict;\n// the other names start with atome_.\nIMPORT://ton.ui\nIMPORT://ton.json\nIMPORT://ton.time\n\nton.let atome_platform = platform()\nton.let atome_os = {\"title\": \"Atome\", \"style\": \"desktop\", \"wins\": [], \"apps\": {}, \"order\": [], \"focus\": \"\", \"next\": 1,\n    \"term\": \"windows\", \"menu\": false, \"msel\": 0, \"hsel\": 0, \"toast\": \"\", \"ttl\": 0, \"w\": 800, \"h\": 600, \"wall\": 0, \"tint\": 1, \"booted\": false, \"home\": true}\n\n// ------------------------------------------------------------------ small helpers\nton.function atome_starts(s, p) { return len(s) >= len(p) and substring(s, 0, len(p)) == p }\nton.function atome_pad2(n) { return n < 10 ? \"0\" + str(n) : str(n) }\nton.function atome_clip(s, n) { return len(s) > n ? substring(s, 0, n) : s }\nton.function atome_spaces(n) { return n > 0 ? \" \" * n : \"\" }\nton.function atome_sort(items) {\n    // insertion sort of strings or numbers, in place\n    ton.let i = 1\n    while i < len(items) {\n        ton.let x = items[i]\n        ton.let j = i - 1\n        while j >= 0 and items[j] > x {\n            items[j + 1] = items[j]\n            j = j - 1\n        }\n        items[j + 1] = x\n        i = i + 1\n    }\n    return items\n}\n// milliseconds since the system started (the kernel's clock, or the browser's)\nton.function atome_now() { return atome_platform == \"web\" ? time_millis() : uptime() }\nton.function atome_time() {\n    if atome_platform == \"web\" {\n        ton.let c = ui_clock()\n        return atome_pad2(c[\"hour\"]) + \":\" + atome_pad2(c[\"minute\"])\n    }\n    ton.let s = uptime() / 1000\n    return atome_pad2(s / 60 % 60) + \":\" + atome_pad2(s % 60)\n}\n\n// ------------------------------------------------------------------ widgets: what an app's view returns\nton.function atome_label(text) { return {\"t\": \"label\", \"text\": text} }\nton.function atome_title(text) { return {\"t\": \"title\", \"text\": text} }\nton.function atome_button(text, act) { return {\"t\": \"button\", \"text\": text, \"act\": act, \"value\": \"\"} }\nton.function atome_item(text, act, value) { return {\"t\": \"item\", \"text\": text, \"act\": act, \"value\": value} }\nton.function atome_input(name, label) { return {\"t\": \"input\", \"name\": name, \"text\": label} }\nton.function atome_gap() { return {\"t\": \"gap\", \"text\": \"\"} }\nton.function atome_rule() { return {\"t\": \"rule\", \"text\": \"\"} }\nton.function atome_in(win, name) { return win[\"s\"].has(\"in_\" + name) ? win[\"s\"][\"in_\" + name] : \"\" }\nton.function atome_set_in(win, name, value) { win[\"s\"][\"in_\" + name] = value }\n\n// ------------------------------------------------------------------ the virtual disk\nton.let atome_dirs = {\"/\": true, \"/home\": true, \"/home/docs\": true, \"/tmp\": true}\nton.let atome_files = {\n    \"/home/readme.txt\": \"Welcome to your system!\\nIt is written in TON.\\nOpen the Terminal and type: help\",\n    \"/home/docs/todo.txt\": \"- write my own app with atome.app(...)\\n- change the look in Settings\"\n}\nton.function atome_parent(path) {\n    ton.let i = len(path) - 1\n    while i > 0 and path[i] != \"/\" { i = i - 1 }\n    return i <= 0 ? \"/\" : substring(path, 0, i)\n}\nton.function atome_base(path) {\n    ton.let i = len(path) - 1\n    while i >= 0 and path[i] != \"/\" { i = i - 1 }\n    return substring(path, i + 1)\n}\nton.function atome_join(dir, name) { return dir == \"/\" ? \"/\" + name : dir + \"/\" + name }\n// the disk as text: one line per folder (\"D<TAB>path\") or file (\"F<TAB>path<TAB>content\"), with %, newlines and tabs escaped\nton.function atome_esc(t) { return replace(replace(replace(t, \"%\", \"%p\"), \"\\n\", \"%n\"), \"\\t\", \"%t\") }\nton.function atome_unesc(t) { return replace(replace(replace(t, \"%t\", \"\\t\"), \"%n\", \"\\n\"), \"%p\", \"%\") }\nton.function atome_serialize() {\n    ton.let lines = []\n    for p in keys(atome_dirs) { push(lines, \"D\\t\" + atome_esc(p)) }\n    for p in keys(atome_files) { push(lines, \"F\\t\" + atome_esc(p) + \"\\t\" + atome_esc(atome_files[p])) }\n    return join(lines, \"\\n\")\n}\nton.function atome_deserialize(text) {\n    ton.let dirs = {}\n    ton.let files = {}\n    for line in split(text, \"\\n\") {\n        ton.let parts = split(line, \"\\t\")\n        if len(parts) >= 2 and parts[0] == \"D\" { dirs[atome_unesc(parts[1])] = true }\n        else if len(parts) >= 3 and parts[0] == \"F\" { files[atome_unesc(parts[1])] = atome_unesc(parts[2]) }\n    }\n    if dirs.has(\"/\") { atome_dirs = dirs\n        atome_files = files\n        return true }\n    return false\n}\n// saved in the browser's storage on the web, on the hard disk on the kernel (if the machine has one)\nton.function atome_save() {\n    if atome_platform == \"web\" { ui_storage_set(\"atome.disk\", json_stringify({\"d\": atome_dirs, \"f\": atome_files})) }\n    else if disk_size() > 0 { disk_save(atome_serialize()) }\n}\nton.function atome_load() {\n    if atome_platform == \"web\" {\n        ton.let saved = ui_storage_get(\"atome.disk\", nil)\n        if saved == nil { return nil }\n        ton.let d = json_parse(saved)\n        if d != nil { atome_dirs = d[\"d\"]\n            atome_files = d[\"f\"] }\n    } else if disk_size() > 0 {\n        ton.let blob = disk_load()\n        if blob != \"\" { atome_deserialize(blob) }\n    }\n}\nton.function atome_exists(path) { return atome_dirs.has(path) or atome_files.has(path) }\nton.function atome_is_dir(path) { return atome_dirs.has(path) }\nton.function atome_read(path) { return atome_files.has(path) ? atome_files[path] : nil }\nton.function atome_write(path, text) {\n    if atome_dirs.has(path) or !atome_dirs.has(atome_parent(path)) { return false }\n    atome_files[path] = text\n    atome_save()\n    return true\n}\nton.function atome_mkdir(path) {\n    if atome_exists(path) or !atome_dirs.has(atome_parent(path)) { return false }\n    atome_dirs[path] = true\n    atome_save()\n    return true\n}\nton.function atome_remove(path) {\n    if path == \"/\" { return false }\n    if atome_files.has(path) { remove(atome_files, path) }\n    else if atome_dirs.has(path) {\n        for p in keys(atome_files) { if atome_starts(p, path + \"/\") { remove(atome_files, p) } }\n        for p in keys(atome_dirs) { if p == path or atome_starts(p, path + \"/\") { remove(atome_dirs, p) } }\n    } else { return false }\n    atome_save()\n    return true\n}\n// what is in a folder: names, folders first, as [{\"name\":..., \"path\":..., \"dir\": true|false}]\nton.function atome_list(dir) {\n    ton.let names = []\n    for p in keys(atome_dirs) { if p != \"/\" and atome_parent(p) == dir { push(names, p) } }\n    atome_sort(names)\n    ton.let out = []\n    for p in names { push(out, {\"name\": atome_base(p), \"path\": p, \"dir\": true}) }\n    ton.let fnames = []\n    for p in keys(atome_files) { if atome_parent(p) == dir { push(fnames, p) } }\n    atome_sort(fnames)\n    for p in fnames { push(out, {\"name\": atome_base(p), \"path\": p, \"dir\": false}) }\n    return out\n}\n\n// ------------------------------------------------------------------ apps and windows\nton.function atome_app(def) {\n    if !def.has(\"w\") { def[\"w\"] = 46 }\n    if !def.has(\"h\") { def[\"h\"] = 14 }\n    if !def.has(\"icon\") { def[\"icon\"] = \"*\" }\n    if !def.has(\"title\") { def[\"title\"] = def[\"id\"] }\n    if !atome_os[\"apps\"].has(def[\"id\"]) { push(atome_os[\"order\"], def[\"id\"]) }\n    atome_os[\"apps\"][def[\"id\"]] = def\n    return nil\n}\nton.function atome_find(id) {\n    for w in atome_os[\"wins\"] { if w[\"id\"] == id { return w } }\n    return nil\n}\nton.function atome_raise(win) {\n    atome_os[\"focus\"] = win[\"id\"]\n    atome_os[\"next\"] = atome_os[\"next\"] + 1\n    win[\"z\"] = atome_os[\"next\"]\n}\nton.function atome_open(app_id) {\n    if !atome_os[\"apps\"].has(app_id) { return nil }\n    ton.let def = atome_os[\"apps\"][app_id]\n    atome_os[\"menu\"] = false\n    atome_os[\"home\"] = false\n    if atome_os[\"style\"] == \"phone\" {            // a phone shows one window per app\n        for w in atome_os[\"wins\"] { if w[\"app\"] == app_id { atome_raise(w)\n            return w } }\n    }\n    ton.let k = len(atome_os[\"wins\"])\n    atome_os[\"next\"] = atome_os[\"next\"] + 1\n    ton.let win = {\"id\": \"w\" + str(atome_os[\"next\"]), \"app\": app_id, \"title\": def[\"title\"], \"icon\": def[\"icon\"], \"s\": {}, \"sel\": 0,\n        \"z\": atome_os[\"next\"], \"x\": 3 + (k * 3) % 18, \"y\": 2 + k % 5, \"w\": def[\"w\"], \"h\": def[\"h\"],\n        \"px\": 70 + (k % 6) * 36, \"py\": 50 + (k % 6) * 32}\n    if def.has(\"init\") { def[\"init\"](win) }\n    push(atome_os[\"wins\"], win)\n    atome_os[\"focus\"] = win[\"id\"]\n    return win\n}\nton.function atome_close(id) {\n    ton.let keep = []\n    for w in atome_os[\"wins\"] { if w[\"id\"] != id { push(keep, w) } }\n    atome_os[\"wins\"] = keep\n    if atome_os[\"focus\"] == id { atome_os[\"focus\"] = len(keep) > 0 ? keep[len(keep) - 1][\"id\"] : \"\" }\n    if len(keep) == 0 { atome_os[\"home\"] = true }\n}\nton.function atome_toast(text) {\n    atome_os[\"toast\"] = text\n    atome_os[\"ttl\"] = 4\n}\nton.function atome_act(win, action, value) {\n    ton.let def = atome_os[\"apps\"][win[\"app\"]]\n    if def.has(\"act\") { def[\"act\"](win, action, value) }\n}\n// Cooperative multitasking: every window whose app has a `tick(win)` function gets it called about once a second,\n// focused or not, so an app can keep working in the background (a stopwatch, a download, a game). Apps take turns.\nton.function atome_tick_all() {\n    ton.let any = false\n    for w in atome_os[\"wins\"] {\n        ton.let def = atome_os[\"apps\"][w[\"app\"]]\n        if def.has(\"tick\") { def[\"tick\"](w)\n            any = true }\n    }\n    return any\n}\nton.function atome_view(win) {\n    ton.let def = atome_os[\"apps\"][win[\"app\"]]\n    return def[\"view\"](win)\n}\nton.function atome_ordered() {\n    // windows from back to front\n    ton.let list = []\n    for w in atome_os[\"wins\"] { push(list, w) }\n    ton.let i = 1\n    while i < len(list) {\n        ton.let x = list[i]\n        ton.let j = i - 1\n        while j >= 0 and list[j][\"z\"] > x[\"z\"] {\n            list[j + 1] = list[j]\n            j = j - 1\n        }\n        list[j + 1] = x\n        i = i + 1\n    }\n    return list\n}\n\n// ------------------------------------------------------------------ the graphical system (Acode, IDE, playground)\nton.let ATOME_CSS = `\n.os{position:absolute;top:0;right:0;bottom:0;left:0;overflow:hidden;color:#fff;font-size:15px}\n.os *{box-sizing:border-box}\n.os button{cursor:pointer;border:0;color:inherit;background:rgba(255,255,255,.16);border-radius:8px;padding:8px 12px;margin:2px 4px 2px 0}\n.os button:active{background:rgba(255,255,255,.3)}\n.os input{width:100%;background:rgba(0,0,0,.35);color:#fff;border:1px solid rgba(255,255,255,.28);border-radius:8px;padding:8px;outline:0}\n.os .wall0{background:linear-gradient(135deg,#1e3c72,#2a5298 55%,#6dd5ed)}\n.os .wall1{background:linear-gradient(135deg,#0f2027,#203a43 50%,#2c5364)}\n.os .wall2{background:linear-gradient(135deg,#42275a,#734b6d)}\n.os .wall3{background:linear-gradient(135deg,#134e5e,#71b280)}\n.os .wall4{background:linear-gradient(135deg,#232526,#414345)}\n.os .wall5{background:linear-gradient(135deg,#ff7e5f,#feb47b)}\n.os .scr{position:absolute;top:0;right:0;bottom:0;left:0}\n.os .win{position:absolute;display:flex;flex-direction:column;border-radius:10px;overflow:hidden;box-shadow:0 10px 40px rgba(0,0,0,.55);border:1px solid rgba(255,255,255,.18);background:#1b1d26}\n.os .win.f{border-color:rgba(255,255,255,.55)}\n.os .tb{display:flex;align-items:center;gap:8px;height:36px;padding:0 6px 0 12px;background:rgba(40,43,56,.98);touch-action:none;cursor:move}\n.os .tb .t{flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-weight:600;font-size:13px}\n.os .tb button{padding:4px 10px;border-radius:6px;background:transparent;margin:0}\n.os .tb button:hover{background:#e5484d}\n.os .wb{flex:1;min-height:0}\n.os .body{padding:12px;height:100%;overflow:auto;background:rgba(24,26,34,.98)}\n.os .l{margin:3px 0;white-space:pre-wrap;word-break:break-word}\n.os .h{font-size:20px;font-weight:600;margin:4px 0 8px}\n.os .item{padding:9px 10px;border-radius:8px;cursor:pointer;margin:1px 0}\n.os .item:hover{background:rgba(255,255,255,.14)}\n.os .inp{margin:6px 0}\n.os .inp label{display:block;font-size:12px;opacity:.7;margin-bottom:3px}\n.os hr{border:0;border-top:1px solid rgba(255,255,255,.18);margin:8px 0}\n.os .tbar{position:absolute;left:0;right:0;bottom:0;height:46px;display:flex;align-items:center;gap:6px;padding:0 8px;background:rgba(15,16,22,.88)}\n.os .tbar .task{max-width:150px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-size:13px}\n.os .tbar .task.on{background:rgba(255,255,255,.32)}\n.os .tbar .g{flex:1}\n.os .menu{position:absolute;left:8px;bottom:54px;width:280px;max-height:70%;overflow:auto;padding:8px;border-radius:12px;background:rgba(22,24,32,.97);box-shadow:0 10px 40px rgba(0,0,0,.6)}\n.os .dsk{position:absolute;top:10px;left:10px;display:grid;grid-template-columns:repeat(2,84px);gap:6px}\n.os .a{display:flex;flex-direction:column;align-items:center;gap:4px;padding:8px 4px;border-radius:8px;text-align:center;font-size:12px;cursor:pointer;text-shadow:0 1px 3px #000}\n.os .a:hover{background:rgba(255,255,255,.16)}\n.os .ico{font-size:24px;font-weight:700;width:46px;height:46px;border-radius:14px;background:rgba(255,255,255,.2);display:flex;align-items:center;justify-content:center}\n.os .sb{position:absolute;top:0;left:0;right:0;height:28px;display:flex;align-items:center;justify-content:space-between;padding:0 14px;font-size:13px;font-weight:600;background:rgba(0,0,0,.3)}\n.os .nav{position:absolute;left:0;right:0;bottom:0;height:46px;display:flex;justify-content:space-around;align-items:center;background:rgba(0,0,0,.65);font-size:20px}\n.os .nav span{padding:8px 34px;cursor:pointer}\n.os .hs{position:absolute;top:28px;left:0;right:0;bottom:46px;padding:20px 14px;overflow:auto}\n.os .grid{display:grid;grid-template-columns:repeat(4,1fr);gap:20px 6px}\n.os .ap{position:absolute;top:28px;left:0;right:0;bottom:46px}\n.os .toast{position:absolute;top:36px;left:50%;transform:translateX(-50%);max-width:92%;padding:10px 16px;border-radius:12px;background:rgba(20,22,30,.97);border:1px solid rgba(255,255,255,.22);box-shadow:0 8px 24px rgba(0,0,0,.5);z-index:99999}\n`\nton.function atome_h(attr) { return ui_escape(attr) }\nton.function atome_h_widget(w, win) {\n    ton.let t = w[\"t\"]\n    ton.let text = ui_escape(w[\"text\"])\n    if t == \"label\" { return \"<div class=l>\" + replace(text, \"\\n\", \"<br>\") + \"</div>\" }\n    if t == \"title\" { return \"<div class=h>\" + text + \"</div>\" }\n    if t == \"gap\" { return \"<div style=height:8px></div>\" }\n    if t == \"rule\" { return \"<hr>\" }\n    ton.let who = \" data-win=\\\"\" + win[\"id\"] + \"\\\"\"\n    if t == \"button\" { return \"<button data-do=\\\"a:\" + atome_h(w[\"act\"]) + \"\\\"\" + who + \" data-value=\\\"\" + atome_h(w[\"value\"]) + \"\\\">\" + text + \"</button>\" }\n    if t == \"item\" { return \"<div class=item data-do=\\\"a:\" + atome_h(w[\"act\"]) + \"\\\"\" + who + \" data-value=\\\"\" + atome_h(w[\"value\"]) + \"\\\">\" + text + \"</div>\" }\n    if t == \"input\" {\n        return \"<div class=inp><label>\" + text + \"</label><input data-bind=\\\"i:\" + atome_h(w[\"name\"]) + \"\\\" data-key=\\\"i:\" + atome_h(w[\"name\"]) + \"\\\"\" + who +\n            \" value=\\\"\" + ui_escape(atome_in(win, w[\"name\"])) + \"\\\" autocomplete=off autocapitalize=off></div>\"\n    }\n    return \"\"\n}\nton.function atome_h_body(win) {\n    ton.let page = \"\"\n    for w in atome_view(win) { page = page + atome_h_widget(w, win) }\n    return page\n}\nton.function atome_h_toast() {\n    return atome_os[\"ttl\"] > 0 ? \"<div class=toast>\" + ui_escape(atome_os[\"toast\"]) + \"</div>\" : \"\"\n}\nton.function atome_h_desktop() {\n    ton.let page = \"<div class=os><div class=\\\"scr wall\" + str(atome_os[\"wall\"]) + \"\\\"><div class=dsk>\"\n    for id in atome_os[\"order\"] {\n        ton.let def = atome_os[\"apps\"][id]\n        page = page + \"<div class=a data-do=\\\"os:launch\\\" data-value=\\\"\" + atome_h(id) + \"\\\"><div class=ico>\" + ui_escape(def[\"icon\"]) + \"</div>\" + ui_escape(def[\"title\"]) + \"</div>\"\n    }\n    page = page + \"</div>\"\n    for win in atome_ordered() {\n        ton.let f = win[\"id\"] == atome_os[\"focus\"] ? \" f\" : \"\"\n        ton.let wpx = win[\"w\"] * 10\n        ton.let hpx = win[\"h\"] * 22 + 36\n        ton.let maxw = atome_os[\"w\"] - 20\n        if wpx > maxw { wpx = maxw }\n        page = page + \"<div class=\\\"win\" + f + \"\\\" data-winbox data-do=\\\"os:focus\\\" data-win=\\\"\" + win[\"id\"] + \"\\\" style=\\\"left:\" + str(win[\"px\"]) + \"px;top:\" + str(win[\"py\"]) + \"px;width:\" +\n            str(wpx) + \"px;height:\" + str(hpx) + \"px;z-index:\" + str(win[\"z\"]) + \"\\\"><div class=tb data-drag=\\\"\" + win[\"id\"] + \"\\\"><span class=t>\" + ui_escape(win[\"icon\"] + \"  \" + win[\"title\"]) +\n            \"</span><button data-do=\\\"os:close\\\" data-win=\\\"\" + win[\"id\"] + \"\\\">✕</button></div><div class=wb><div class=body data-scroll=\\\"\" + win[\"id\"] + \"\\\">\" + atome_h_body(win) + \"</div></div></div>\"\n    }\n    page = page + \"<div class=tbar><button data-do=\\\"os:start\\\">☰ \" + ui_escape(atome_os[\"title\"]) + \"</button>\"\n    for win in atome_os[\"wins\"] {\n        page = page + \"<button class=\\\"task\" + (win[\"id\"] == atome_os[\"focus\"] ? \" on\" : \"\") + \"\\\" data-do=\\\"os:focus\\\" data-win=\\\"\" + win[\"id\"] + \"\\\">\" + ui_escape(win[\"icon\"] + \" \" + win[\"title\"]) + \"</button>\"\n    }\n    page = page + \"<span class=g></span><span>\" + atome_time() + \"</span></div>\"\n    if atome_os[\"menu\"] {\n        page = page + \"<div class=menu>\"\n        for id in atome_os[\"order\"] {\n            page = page + \"<div class=item data-do=\\\"os:launch\\\" data-value=\\\"\" + atome_h(id) + \"\\\">\" + ui_escape(atome_os[\"apps\"][id][\"icon\"] + \"   \" + atome_os[\"apps\"][id][\"title\"]) + \"</div>\"\n        }\n        page = page + \"<hr><div class=item data-do=\\\"os:style\\\" data-value=\\\"phone\\\">📱  Phone screen</div></div>\"\n    }\n    return page + atome_h_toast() + \"</div></div>\"\n}\nton.function atome_h_phone() {\n    ton.let page = \"<div class=os><div class=\\\"scr wall\" + str(atome_os[\"wall\"]) + \"\\\"><div class=sb><span>\" + atome_time() + \"</span><span>\" + ui_escape(atome_os[\"title\"]) + \"</span></div>\"\n    ton.let win = atome_find(atome_os[\"focus\"])\n    if atome_os[\"menu\"] {                            // recent apps\n        page = page + \"<div class=hs><div class=h>Recent apps</div>\"\n        for w in atome_os[\"wins\"] {\n            page = page + \"<div class=item data-do=\\\"os:focus\\\" data-win=\\\"\" + w[\"id\"] + \"\\\">\" + ui_escape(w[\"icon\"] + \"   \" + w[\"title\"]) + \"</div>\"\n        }\n        if len(atome_os[\"wins\"]) == 0 { page = page + \"<div class=l>No app is open.</div>\" }\n        page = page + \"</div>\"\n    } else if atome_os[\"home\"] or win == nil {\n        page = page + \"<div class=hs><div class=h style=\\\"font-size:46px;font-weight:300\\\">\" + atome_time() + \"</div><div class=grid>\"\n        for id in atome_os[\"order\"] {\n            ton.let def = atome_os[\"apps\"][id]\n            page = page + \"<div class=a data-do=\\\"os:launch\\\" data-value=\\\"\" + atome_h(id) + \"\\\"><div class=ico>\" + ui_escape(def[\"icon\"]) + \"</div>\" + ui_escape(def[\"title\"]) + \"</div>\"\n        }\n        page = page + \"</div></div>\"\n    } else {\n        page = page + \"<div class=ap><div class=body style=\\\"position:absolute;top:0;right:0;bottom:0;left:0\\\" data-scroll=\\\"\" + win[\"id\"] + \"\\\">\" + atome_h_body(win) + \"</div></div>\"\n    }\n    page = page + \"<div class=nav><span data-do=\\\"os:back\\\">◀</span><span data-do=\\\"os:home\\\">●</span><span data-do=\\\"os:recents\\\">■</span></div>\"\n    return page + atome_h_toast() + \"</div></div>\"\n}\nton.function atome_h_custom() {\n    ton.let win = atome_find(atome_os[\"focus\"])\n    if win == nil and len(atome_os[\"order\"]) > 0 { win = atome_open(atome_os[\"order\"][0]) }\n    return \"<div class=os><div class=\\\"scr wall\" + str(atome_os[\"wall\"]) + \"\\\"><div class=body style=\\\"position:absolute;top:0;right:0;bottom:0;left:0\\\" data-scroll=\\\"\" + (win == nil ? \"\" : win[\"id\"]) + \"\\\">\" +\n        (win == nil ? \"\" : atome_h_body(win)) + \"</div>\" + atome_h_toast() + \"</div></div>\"\n}\nton.function atome_web_draw() {\n    ton.let style = atome_os[\"style\"]\n    ui_render(\"<style>\" + ATOME_CSS + \"</style>\" + (style == \"phone\" ? atome_h_phone() : (style == \"custom\" ? atome_h_custom() : atome_h_desktop())))\n}\nton.function atome_web_event(e) {\n    ton.let t = e[\"type\"]\n    ton.let id = e[\"id\"]\n    if t == \"size\" {\n        atome_os[\"w\"] = e[\"x\"]\n        atome_os[\"h\"] = e[\"y\"]\n        if atome_os[\"auto\"] { atome_os[\"style\"] = e[\"x\"] < 700 ? \"phone\" : \"desktop\" }\n        atome_web_draw()\n        return nil\n    }\n    if t == \"move\" {\n        ton.let w = atome_find(e[\"win\"])\n        if w != nil { w[\"px\"] = e[\"x\"]\n            w[\"py\"] = e[\"y\"] }\n        return nil\n    }\n    if t == \"click\" and atome_starts(id, \"os:\") {\n        ton.let what = substring(id, 3)\n        ton.let win = atome_find(e[\"win\"])\n        if what == \"close\" and win != nil { atome_close(win[\"id\"]) }\n        else if what == \"focus\" and win != nil {\n            if atome_os[\"focus\"] == win[\"id\"] and !atome_os[\"menu\"] and atome_os[\"style\"] != \"phone\" { return nil }\n            atome_raise(win)\n            atome_os[\"menu\"] = false\n            atome_os[\"home\"] = false\n        }\n        else if what == \"launch\" { atome_open(e[\"value\"]) }\n        else if what == \"start\" { atome_os[\"menu\"] = !atome_os[\"menu\"] }\n        else if what == \"home\" { atome_os[\"home\"] = true\n            atome_os[\"menu\"] = false }\n        else if what == \"recents\" { atome_os[\"menu\"] = !atome_os[\"menu\"] }\n        else if what == \"back\" {\n            if atome_os[\"menu\"] { atome_os[\"menu\"] = false }\n            else if win == nil and atome_os[\"focus\"] != \"\" { atome_close(atome_os[\"focus\"]) }\n            else { atome_os[\"home\"] = true }\n        }\n        else if what == \"style\" { atome_os[\"style\"] = e[\"value\"]\n            atome_os[\"menu\"] = false }\n        atome_web_draw()\n        return nil\n    }\n    ton.let win = atome_find(e[\"win\"])\n    if win == nil { return nil }\n    if t == \"click\" and atome_starts(id, \"a:\") {\n        atome_act(win, substring(id, 2), e[\"value\"])\n        atome_web_draw()\n    } else if t == \"input\" and atome_starts(id, \"i:\") {\n        atome_set_in(win, substring(id, 2), e[\"value\"])\n    } else if t == \"key\" and atome_starts(id, \"i:\") and e[\"key\"] == \"Enter\" {\n        atome_set_in(win, substring(id, 2), e[\"value\"])\n        atome_act(win, \"enter:\" + substring(id, 2), e[\"value\"])\n        atome_web_draw()\n    }\n    return nil\n}\nton.function atome_web_tick() {\n    ton.let minute = atome_time()\n    ton.let changed = minute != atome_os[\"min\"]\n    atome_os[\"min\"] = minute\n    if atome_tick_all() { changed = true }\n    if atome_os[\"ttl\"] > 0 { atome_os[\"ttl\"] = atome_os[\"ttl\"] - 1\n        changed = true }\n    if changed { atome_web_draw() }\n}\n\n// ------------------------------------------------------------------ the text-mode system (Atome kernel)\n// Windows style: Esc opens the menu of apps, Tab changes window, Up/Down choose, Enter activates, typing fills the\n// selected field. Android style: a home screen of apps (arrows + Enter), Esc goes back home. Custom: your app fills\n// the screen and the system adds nothing.\nton.function atome_k_widgets(win) {\n    ton.let list = []\n    for w in atome_view(win) { push(list, w) }\n    if atome_os[\"style\"] != \"custom\" { push(list, {\"t\": \"button\", \"text\": atome_os[\"style\"] == \"phone\" ? \"Back\" : \"Close window\", \"act\": \"os:close\", \"value\": \"\"}) }\n    return list\n}\nton.function atome_k_focusables(widgets) {\n    ton.let out = []\n    for w in widgets { if w[\"t\"] == \"button\" or w[\"t\"] == \"item\" or w[\"t\"] == \"input\" { push(out, w) } }\n    return out\n}\nton.function atome_k_row(w, win, selected, iw) {\n    ton.let t = w[\"t\"]\n    if t == \"label\" { return atome_clip(w[\"text\"], iw) }\n    if t == \"title\" { return atome_clip(upper(w[\"text\"]), iw) }\n    if t == \"rule\" { return \"-\" * iw }\n    if t == \"button\" { return atome_clip(\"[ \" + w[\"text\"] + \" ]\", iw) }\n    if t == \"item\" { return atome_clip((selected ? \"> \" : \"  \") + w[\"text\"], iw) }\n    if t == \"input\" { return atome_clip(w[\"text\"] + \": \" + atome_in(win, w[\"name\"]) + (selected ? \"_\" : \"\"), iw) }\n    return \"\"\n}\n// the widgets of a window, as text rows inside the rectangle (x, y, w, h)\nton.function atome_k_rows(win, x, y, w, h, focused) {\n    ton.let widgets = atome_k_widgets(win)\n    ton.let foc = atome_k_focusables(widgets)\n    if len(foc) == 0 { foc = [{\"t\": \"none\"}] }\n    if win[\"sel\"] >= len(foc) { win[\"sel\"] = len(foc) - 1 }\n    if win[\"sel\"] < 0 { win[\"sel\"] = 0 }\n    ton.let selw = foc[win[\"sel\"]]\n    ton.let rows = []\n    ton.let selrow = 0\n    for wd in widgets {\n        ton.let isel = focused and wd == selw\n        if isel { selrow = len(rows) }\n        push(rows, {\"text\": atome_k_row(wd, win, isel, w), \"sel\": isel})\n    }\n    ton.let first = 0\n    if selrow >= h { first = selrow - h + 1 }\n    ton.let n = 0\n    while n < h and first + n < len(rows) {\n        ton.let row = rows[first + n]\n        put(x, y + n, row[\"text\"], row[\"sel\"] ? color(15, 2) : color(0, 7))\n        n = n + 1\n    }\n}\nton.function atome_k_window(win) {\n    ton.let focused = win[\"id\"] == atome_os[\"focus\"]\n    ton.let w = win[\"w\"]\n    ton.let h = win[\"h\"]\n    if w > 76 { w = 76 }\n    if h > 21 { h = 21 }\n    ton.let x = win[\"x\"]\n    ton.let y = win[\"y\"] + 1\n    if x + w > 80 { x = 80 - w }\n    if y + h > 24 { y = 24 - h }\n    ton.let frame = focused ? color(15, 1) : color(7, 0)\n    fill(x, y, w, h, \" \", color(0, 7))\n    put(x, y, char(218) + char(196) * (w - 2) + char(191), frame)\n    put(x, y + h - 1, char(192) + char(196) * (w - 2) + char(217), frame)\n    for r in range(1, h - 1) {\n        put(x, y + r, char(179), frame)\n        put(x + w - 1, y + r, char(179), frame)\n    }\n    put(x + 2, y, \" \" + atome_clip(win[\"title\"], w - 6) + \" \", frame)\n    atome_k_rows(win, x + 2, y + 1, w - 4, h - 2, focused)\n}\nton.function atome_k_current() {\n    ton.let win = atome_find(atome_os[\"focus\"])\n    if atome_os[\"style\"] == \"custom\" and win == nil and len(atome_os[\"order\"]) > 0 { win = atome_open(atome_os[\"order\"][0]) }\n    return win\n}\nton.function atome_k_draw() {\n    ton.let style = atome_os[\"style\"]\n    ton.let tint = color(1 + atome_os[\"wall\"] % 6, 0)\n    if style == \"custom\" {\n        ton.let win = atome_k_current()\n        fill(0, 0, 80, 25, \" \", color(0, 7))\n        if win != nil { atome_k_rows(win, 1, 0, 78, 25, true) }\n        return nil\n    }\n    fill(0, 0, 80, 25, char(176), tint)\n    fill(0, 0, 80, 1, \" \", color(15, 1))\n    put(1, 0, atome_os[\"title\"], color(15, 1))\n    put(74, 0, atome_time(), color(15, 1))\n    if style == \"phone\" {\n        ton.let win = atome_find(atome_os[\"focus\"])\n        if atome_os[\"home\"] or win == nil {\n            fill(2, 2, 76, 21, \" \", color(0, 7))\n            put(4, 3, \"HOME\", color(0, 7))\n            ton.let i = 0\n            for id in atome_os[\"order\"] {\n                ton.let col = i % 3\n                ton.let row = i / 3 - (i % 3) / 3\n                put(6 + col * 24, 6 + row * 3, (i == atome_os[\"hsel\"] ? \"> \" : \"  \") + \"[\" + atome_os[\"apps\"][id][\"icon\"] + \"] \" + atome_clip(atome_os[\"apps\"][id][\"title\"], 14),\n                    i == atome_os[\"hsel\"] ? color(15, 2) : color(0, 7))\n                i = i + 1\n            }\n            put(4, 21, \"Arrows choose, Enter opens\", color(0, 7))\n        } else {\n            fill(0, 1, 80, 23, \" \", color(0, 7))\n            put(1, 1, atome_clip(win[\"title\"], 60), color(15, 4))\n            atome_k_rows(win, 2, 3, 76, 20, true)\n        }\n        fill(0, 24, 80, 1, \" \", color(0, 7))\n        put(1, 24, \"Esc: home   Up/Down: choose   Enter: ok\", color(0, 7))\n    } else {\n        for win in atome_ordered() { atome_k_window(win) }\n        fill(0, 24, 80, 1, \" \", color(0, 7))\n        put(1, 24, \"Esc menu  Tab window  Up/Down choose  Enter ok\", color(0, 7))\n        if atome_os[\"menu\"] {\n            ton.let n = len(atome_os[\"order\"]) + 2\n            ton.let mx = 24\n            ton.let my = 3\n            fill(mx, my, 32, n + 2, \" \", color(0, 7))\n            put(mx, my, char(218) + char(196) * 30 + char(191), color(15, 1))\n            put(mx, my + n + 1, char(192) + char(196) * 30 + char(217), color(15, 1))\n            ton.let i = 0\n            while i < n {\n                ton.let label = \"\"\n                if i < len(atome_os[\"order\"]) { label = atome_os[\"apps\"][atome_os[\"order\"][i]][\"title\"] }\n                else if i == len(atome_os[\"order\"]) { label = \"Power off\" }\n                else { label = \"Reboot\" }\n                put(mx + 2, my + 1 + i, (i == atome_os[\"msel\"] ? \"> \" : \"  \") + atome_clip(label, 26), i == atome_os[\"msel\"] ? color(15, 2) : color(0, 7))\n                i = i + 1\n            }\n        }\n    }\n    if atome_os[\"ttl\"] > 0 { put(2, 23, \" \" + atome_clip(atome_os[\"toast\"], 70) + \" \", color(0, 14)) }\n}\n// keys inside a window: choose a widget, press it, type in a field\nton.function atome_k_window_key(win, k) {\n    ton.let foc = atome_k_focusables(atome_k_widgets(win))\n    if len(foc) == 0 { return nil }\n    ton.let sel = win[\"sel\"]\n    if sel >= len(foc) { sel = len(foc) - 1 }\n    ton.let cur = foc[sel]\n    if k == \"up\" { win[\"sel\"] = (sel + len(foc) - 1) % len(foc) }\n    else if k == \"down\" { win[\"sel\"] = (sel + 1) % len(foc) }\n    else if cur[\"t\"] == \"input\" {\n        ton.let value = atome_in(win, cur[\"name\"])\n        if k == \"enter\" { atome_act(win, \"enter:\" + cur[\"name\"], value) }\n        else if k == \"backspace\" { if len(value) > 0 { atome_set_in(win, cur[\"name\"], substring(value, 0, len(value) - 1)) } }\n        else if len(k) == 1 { atome_set_in(win, cur[\"name\"], value + k) }\n    }\n    else if k == \"enter\" {\n        if cur[\"act\"] == \"os:close\" { atome_close(win[\"id\"]) }\n        else { atome_act(win, cur[\"act\"], cur[\"value\"]) }\n    }\n    return nil\n}\nton.function atome_k_key(k) {\n    if atome_os[\"ttl\"] > 0 { atome_os[\"ttl\"] = atome_os[\"ttl\"] - 1 }\n    ton.let style = atome_os[\"style\"]\n    if style == \"custom\" {\n        ton.let w = atome_k_current()\n        if w != nil { atome_k_window_key(w, k) }\n        return nil\n    }\n    if style == \"phone\" {\n        ton.let win = atome_find(atome_os[\"focus\"])\n        if atome_os[\"home\"] or win == nil {\n            ton.let n = len(atome_os[\"order\"])\n            if k == \"right\" or k == \"down\" { atome_os[\"hsel\"] = (atome_os[\"hsel\"] + (k == \"down\" ? 3 : 1)) % n }\n            else if k == \"left\" or k == \"up\" { atome_os[\"hsel\"] = (atome_os[\"hsel\"] + n - (k == \"up\" ? 3 : 1) % n) % n }\n            else if k == \"enter\" { atome_open(atome_os[\"order\"][atome_os[\"hsel\"]]) }\n        } else if k == \"esc\" { atome_os[\"home\"] = true }\n        else { atome_k_window_key(win, k) }\n        return nil\n    }\n    if atome_os[\"menu\"] {\n        ton.let n = len(atome_os[\"order\"]) + 2\n        if k == \"up\" { atome_os[\"msel\"] = (atome_os[\"msel\"] + n - 1) % n }\n        else if k == \"down\" { atome_os[\"msel\"] = (atome_os[\"msel\"] + 1) % n }\n        else if k == \"esc\" { atome_os[\"menu\"] = false }\n        else if k == \"enter\" {\n            ton.let i = atome_os[\"msel\"]\n            if i < len(atome_os[\"order\"]) { atome_open(atome_os[\"order\"][i]) }\n            else if i == len(atome_os[\"order\"]) { poweroff() }\n            else { reboot() }\n        }\n        return nil\n    }\n    if k == \"esc\" { atome_os[\"menu\"] = true\n        atome_os[\"msel\"] = 0\n        return nil }\n    if k == \"tab\" {\n        if len(atome_os[\"wins\"]) > 0 {\n            ton.let ws = atome_ordered()\n            atome_raise(ws[0])          // the window at the back comes to the front\n        }\n        return nil\n    }\n    ton.let win = atome_find(atome_os[\"focus\"])\n    if win != nil { atome_k_window_key(win, k) }\n    return nil\n}\nton.function atome_k_run() {\n    while true {\n        atome_k_draw()\n        ton.let k = poll()\n        ton.let last = atome_now()\n        while k == \"\" {                         // nothing pressed: let the apps with a tick work, about once a second\n            sleep(20)\n            if atome_now() - last >= 1000 {\n                last = atome_now()\n                if atome_tick_all() { atome_k_draw() }\n            }\n            k = poll()\n        }\n        atome_k_key(k)\n    }\n}\n\n// ------------------------------------------------------------------ the apps every system starts with\n// ---- Notes: a list of short notes, saved in /home/notes.txt\nton.function atome_notes_save(win) { atome_write(\"/home/notes.txt\", join(win[\"s\"][\"notes\"], \"\\n\")) }\nton.function atome_notes_init(win) {\n    ton.let text = atome_read(\"/home/notes.txt\")\n    win[\"s\"][\"notes\"] = (text == nil or text == \"\") ? [] : split(text, \"\\n\")\n}\nton.function atome_notes_view(win) {\n    ton.let w = [atome_title(\"Notes\"), atome_input(\"note\", \"New note (Enter to save)\")]\n    ton.let i = 0\n    for n in win[\"s\"][\"notes\"] {\n        push(w, atome_item(\"- \" + n, \"del\", str(i)))\n        i = i + 1\n    }\n    if len(win[\"s\"][\"notes\"]) == 0 { push(w, atome_label(\"No notes yet.\")) }\n    else { push(w, atome_label(\"Choose a note and press Enter to delete it.\")) }\n    return w\n}\nton.function atome_notes_act(win, action, value) {\n    if action == \"enter:note\" {\n        ton.let text = atome_in(win, \"note\")\n        if len(text) > 0 { push(win[\"s\"][\"notes\"], text)\n            atome_set_in(win, \"note\", \"\")\n            atome_notes_save(win) }\n    } else if action == \"del\" {\n        ton.let keep = []\n        ton.let i = 0\n        for n in win[\"s\"][\"notes\"] {\n            if str(i) != value { push(keep, n) }\n            i = i + 1\n        }\n        win[\"s\"][\"notes\"] = keep\n        atome_notes_save(win)\n    }\n}\n\n// ---- Calculator: + - * / % and parentheses, on whole numbers\nton.let atome_ce_s = \"\"\nton.let atome_ce_i = 0\nton.let atome_ce_err = \"\"\nton.function atome_ce_skip() { while atome_ce_i < len(atome_ce_s) and atome_ce_s[atome_ce_i] == \" \" { atome_ce_i += 1 } }\nton.function atome_ce_atom() {\n    atome_ce_skip()\n    if atome_ce_i >= len(atome_ce_s) { atome_ce_err = \"the expression ends too soon\"\n        return 0 }\n    ton.let c = atome_ce_s[atome_ce_i]\n    if c == \"(\" {\n        atome_ce_i += 1\n        ton.let v = atome_ce_sum()\n        atome_ce_skip()\n        if atome_ce_i < len(atome_ce_s) and atome_ce_s[atome_ce_i] == \")\" { atome_ce_i += 1 } else { atome_ce_err = \"missing )\" }\n        return v\n    }\n    if c == \"-\" {\n        atome_ce_i += 1\n        return 0 - atome_ce_atom()\n    }\n    if !contains(\"0123456789\", c) { atome_ce_err = \"unexpected \" + c\n        atome_ce_i = len(atome_ce_s)\n        return 0 }\n    ton.let n = 0\n    while atome_ce_i < len(atome_ce_s) and contains(\"0123456789\", atome_ce_s[atome_ce_i]) {\n        n = n * 10 + num(atome_ce_s[atome_ce_i])\n        atome_ce_i += 1\n    }\n    return n\n}\nton.function atome_ce_product() {\n    ton.let v = atome_ce_atom()\n    atome_ce_skip()\n    while atome_ce_i < len(atome_ce_s) and contains(\"*/%\", atome_ce_s[atome_ce_i]) {\n        ton.let op = atome_ce_s[atome_ce_i]\n        atome_ce_i += 1\n        ton.let r = atome_ce_atom()\n        if op == \"*\" { v = v * r }\n        else if r == 0 { atome_ce_err = \"division by zero\" }\n        else if op == \"/\" { v = v / r }\n        else { v = v % r }\n        atome_ce_skip()\n    }\n    return v\n}\nton.function atome_ce_sum() {\n    ton.let v = atome_ce_product()\n    atome_ce_skip()\n    while atome_ce_i < len(atome_ce_s) and contains(\"+-\", atome_ce_s[atome_ce_i]) {\n        ton.let op = atome_ce_s[atome_ce_i]\n        atome_ce_i += 1\n        ton.let r = atome_ce_product()\n        if op == \"+\" { v = v + r } else { v = v - r }\n        atome_ce_skip()\n    }\n    return v\n}\nton.function atome_calc(text) {\n    atome_ce_s = text\n    atome_ce_i = 0\n    atome_ce_err = \"\"\n    ton.let v = atome_ce_sum()\n    atome_ce_skip()\n    if atome_ce_err == \"\" and atome_ce_i < len(atome_ce_s) { atome_ce_err = \"unexpected \" + atome_ce_s[atome_ce_i] }\n    return atome_ce_err == \"\" ? str(v) : \"error: \" + atome_ce_err\n}\nton.function atome_calc_init(win) {\n    win[\"s\"][\"hist\"] = []\n    win[\"s\"][\"res\"] = \"\"\n}\nton.function atome_calc_view(win) {\n    ton.let w = [atome_title(\"Calculator\"), atome_input(\"expr\", \"Expression, for example 12*(3+4)\"), atome_button(\"Calculate\", \"calc\"), atome_rule()]\n    if win[\"s\"][\"res\"] != \"\" { push(w, atome_title(\"= \" + win[\"s\"][\"res\"])) }\n    for h in win[\"s\"][\"hist\"] { push(w, atome_label(h)) }\n    return w\n}\nton.function atome_calc_act(win, action, value) {\n    if action == \"calc\" or action == \"enter:expr\" {\n        ton.let text = atome_in(win, \"expr\")\n        if len(text) == 0 { return nil }\n        ton.let r = atome_calc(text)\n        win[\"s\"][\"res\"] = r\n        push(win[\"s\"][\"hist\"], text + \" = \" + r)\n        if len(win[\"s\"][\"hist\"]) > 6 { win[\"s\"][\"hist\"] = win[\"s\"][\"hist\"][1:len(win[\"s\"][\"hist\"])] }\n        atome_set_in(win, \"expr\", \"\")\n    }\n}\n\n// ---- Files: browse, create and delete files and folders of the virtual disk\nton.function atome_files_init(win) {\n    win[\"s\"][\"dir\"] = \"/home\"\n    win[\"s\"][\"file\"] = \"\"\n}\nton.function atome_files_view(win) {\n    if win[\"s\"][\"file\"] != \"\" {\n        ton.let text = atome_read(win[\"s\"][\"file\"])\n        ton.let w = [atome_title(atome_base(win[\"s\"][\"file\"])), atome_rule()]\n        for line in split(text == nil ? \"\" : text, \"\\n\") { push(w, atome_label(line)) }\n        push(w, atome_rule())\n        push(w, atome_button(\"Back\", \"close\"))\n        push(w, atome_button(\"Delete this file\", \"delete\"))\n        return w\n    }\n    ton.let w = [atome_title(win[\"s\"][\"dir\"])]\n    if win[\"s\"][\"dir\"] != \"/\" { push(w, atome_item(\"[..]  up\", \"up\", \"\")) }\n    for e in atome_list(win[\"s\"][\"dir\"]) { push(w, atome_item((e[\"dir\"] ? \"[+] \" : \"    \") + e[\"name\"], \"open\", e[\"path\"])) }\n    push(w, atome_input(\"new\", \"New file, or folder if it ends with /\"))\n    return w\n}\nton.function atome_files_act(win, action, value) {\n    if action == \"up\" { win[\"s\"][\"dir\"] = atome_parent(win[\"s\"][\"dir\"]) }\n    else if action == \"open\" {\n        if atome_is_dir(value) { win[\"s\"][\"dir\"] = value } else { win[\"s\"][\"file\"] = value }\n    } else if action == \"close\" { win[\"s\"][\"file\"] = \"\" }\n    else if action == \"delete\" {\n        atome_remove(win[\"s\"][\"file\"])\n        win[\"s\"][\"file\"] = \"\"\n    } else if action == \"enter:new\" {\n        ton.let name = atome_in(win, \"new\")\n        if len(name) > 0 {\n            if name[len(name) - 1] == \"/\" { atome_mkdir(atome_join(win[\"s\"][\"dir\"], substring(name, 0, len(name) - 1))) }\n            else { atome_write(atome_join(win[\"s\"][\"dir\"], name), \"\") }\n            atome_set_in(win, \"new\", \"\")\n        }\n    }\n}\n\n// ---- Terminal: two styles, Windows (cmd) and Unix, switchable with the `unix` and `cmd` commands or in Settings\n// a path like C:\\home\\docs, docs\\x.txt, ..\\ or /home turned into an absolute path of the disk\nton.function atome_resolve(cwd, arg) {\n    ton.let a = replace(arg, \"\\\\\", \"/\")\n    if len(a) >= 2 and a[1] == \":\" { a = substring(a, 2) }\n    ton.let parts = a != \"\" and a[0] == \"/\" ? [] : split(cwd, \"/\")\n    ton.let stack = []\n    for p in parts { if p != \"\" { push(stack, p) } }\n    for p in split(a, \"/\") {\n        if p == \"..\" { pop(stack) }\n        else if p != \"\" and p != \".\" { push(stack, p) }\n    }\n    return \"/\" + join(stack, \"/\")\n}\nton.function atome_winpath(path) { return \"C:\" + (path == \"/\" ? \"\\\\\" : replace(path, \"/\", \"\\\\\")) }\nton.function atome_tree(out, dir, indent) {\n    for e in atome_list(dir) {\n        push(out, indent + (e[\"dir\"] ? \"+--- \" : \"|    \") + e[\"name\"])\n        if e[\"dir\"] { atome_tree(out, e[\"path\"], indent + \"|    \") }\n    }\n}\nton.function atome_cmd(win, line) {\n    ton.let out = win[\"s\"][\"out\"]\n    push(out, atome_winpath(win[\"s\"][\"cwd\"]) + \">\" + line)\n    ton.let words = split(trim(line), \" \")\n    ton.let cmd = lower(words[0])\n    ton.let arg = len(words) > 1 ? join(words[1:len(words)], \" \") : \"\"\n    if cmd == \"\" { return nil }\n    if cmd == \"cd..\" { cmd = \"cd\"\n        arg = \"..\" }\n    if cmd == \"cd\\\\\" { cmd = \"cd\"\n        arg = \"\\\\\" }\n    if cmd == \"help\" {\n        push(out, \"DIR  CD  CLS  TYPE  MD  RD  DEL  COPY  REN  MOVE  ECHO  TREE\")\n        push(out, \"VER  DATE  TIME  HOSTNAME  WHOAMI  TASKLIST  TASKKILL  START  TITLE  EXIT  UNIX\")\n    } else if cmd == \"ver\" { push(out, \"Atome [Version 1.0] on \" + atome_platform)\n    } else if cmd == \"cls\" { win[\"s\"][\"out\"] = []\n    } else if cmd == \"echo\" { push(out, arg)\n    } else if cmd == \"date\" or cmd == \"time\" { push(out, \"The current time is: \" + atome_time())\n    } else if cmd == \"hostname\" { push(out, atome_os[\"title\"])\n    } else if cmd == \"whoami\" { push(out, lower(atome_os[\"title\"]) + \"\\\\user\")\n    } else if cmd == \"exit\" { atome_close(win[\"id\"])\n    } else if cmd == \"title\" { win[\"title\"] = arg\n    } else if cmd == \"unix\" or cmd == \"sh\" { win[\"s\"][\"mode\"] = \"unix\"\n        push(out, \"Unix style. Type cmd to come back.\")\n    } else if cmd == \"cd\" or cmd == \"chdir\" {\n        if arg == \"\" { push(out, atome_winpath(win[\"s\"][\"cwd\"])) }\n        else {\n            ton.let p = atome_resolve(win[\"s\"][\"cwd\"], arg)\n            if atome_is_dir(p) { win[\"s\"][\"cwd\"] = p } else { push(out, \"The system cannot find the path specified.\") }\n        }\n    } else if cmd == \"dir\" {\n        ton.let p = atome_resolve(win[\"s\"][\"cwd\"], arg)\n        if !atome_is_dir(p) { push(out, \"File Not Found\") }\n        else {\n            push(out, \" Directory of \" + atome_winpath(p))\n            push(out, \"\")\n            ton.let files = 0\n            ton.let dirs = 0\n            for e in atome_list(p) {\n                if e[\"dir\"] { dirs = dirs + 1\n                    push(out, \"<DIR>          \" + e[\"name\"]) }\n                else { files = files + 1\n                    push(out, atome_spaces(14 - len(str(len(atome_read(e[\"path\"]))))) + str(len(atome_read(e[\"path\"]))) + \" \" + e[\"name\"]) }\n            }\n            push(out, str(files) + \" File(s)   \" + str(dirs) + \" Dir(s)\")\n        }\n    } else if cmd == \"type\" {\n        ton.let t = atome_read(atome_resolve(win[\"s\"][\"cwd\"], arg))\n        if t == nil { push(out, \"The system cannot find the file specified.\") } else { for l in split(t, \"\\n\") { push(out, l) } }\n    } else if cmd == \"md\" or cmd == \"mkdir\" {\n        if !atome_mkdir(atome_resolve(win[\"s\"][\"cwd\"], arg)) { push(out, \"A subdirectory or file already exists.\") }\n    } else if cmd == \"rd\" or cmd == \"rmdir\" {\n        ton.let p = atome_resolve(win[\"s\"][\"cwd\"], arg)\n        if atome_is_dir(p) { atome_remove(p) } else { push(out, \"The system cannot find the file specified.\") }\n    } else if cmd == \"del\" or cmd == \"erase\" {\n        ton.let p = atome_resolve(win[\"s\"][\"cwd\"], arg)\n        if atome_read(p) != nil { atome_remove(p) } else { push(out, \"Could Not Find \" + atome_winpath(p)) }\n    } else if cmd == \"copy\" or cmd == \"move\" or cmd == \"ren\" or cmd == \"rename\" {\n        ton.let two = split(arg, \" \")\n        if len(two) < 2 { push(out, \"The syntax of the command is incorrect.\") }\n        else {\n            ton.let src = atome_resolve(win[\"s\"][\"cwd\"], two[0])\n            ton.let dst = atome_resolve(win[\"s\"][\"cwd\"], two[1])\n            ton.let renaming = cmd == \"ren\" or cmd == \"rename\"\n            if renaming and !contains(two[1], \"\\\\\") and !contains(two[1], \"/\") { dst = atome_join(atome_parent(src), two[1]) }\n            if atome_is_dir(dst) { dst = atome_join(dst, atome_base(src)) }\n            ton.let text = atome_read(src)\n            if text == nil { push(out, \"The system cannot find the file specified.\") }\n            else if !atome_write(dst, text) { push(out, \"The system cannot find the path specified.\") }\n            else if cmd != \"copy\" { atome_remove(src) }\n            else { push(out, \"        1 file(s) copied.\") }\n        }\n    } else if cmd == \"tree\" {\n        push(out, \"Folder PATH listing\")\n        push(out, atome_winpath(win[\"s\"][\"cwd\"]))\n        atome_tree(out, win[\"s\"][\"cwd\"], \"\")\n    } else if cmd == \"tasklist\" {\n        push(out, \"Image Name                PID\")\n        for w in atome_os[\"wins\"] { push(out, atome_clip(w[\"title\"] + atome_spaces(24), 24) + \"  \" + w[\"id\"]) }\n    } else if cmd == \"taskkill\" {\n        ton.let id = replace(replace(lower(arg), \"/pid \", \"\"), \" \", \"\")\n        if atome_find(id) == nil { push(out, \"ERROR: The process \\\"\" + id + \"\\\" not found.\") }\n        else { atome_close(id)\n            push(out, \"SUCCESS: The process with PID \" + id + \" has been terminated.\") }\n    } else if cmd == \"start\" { if atome_open(arg) == nil { push(out, \"The system cannot find the file \" + arg + \".\") }\n    } else { push(out, \"'\" + words[0] + \"' is not recognized as an internal or external command.\") }\n    if len(win[\"s\"][\"out\"]) > 40 { win[\"s\"][\"out\"] = win[\"s\"][\"out\"][len(win[\"s\"][\"out\"]) - 40:len(win[\"s\"][\"out\"])] }\n}\nton.function atome_sh_path(win, arg) { return atome_resolve(win[\"s\"][\"cwd\"], arg) }\nton.function atome_sh(win, line) {\n    ton.let out = win[\"s\"][\"out\"]\n    push(out, \"$ \" + line)\n    ton.let words = split(trim(line), \" \")\n    ton.let cmd = words[0]\n    ton.let arg = len(words) > 1 ? join(words[1:len(words)], \" \") : \"\"\n    if cmd == \"\" { return nil }\n    if cmd == \"help\" { push(out, \"help ls cd pwd cat mkdir touch rm echo clear apps open ps kill uname time neofetch cmd\") }\n    else if cmd == \"cmd\" or cmd == \"windows\" { win[\"s\"][\"mode\"] = \"windows\"\n        push(out, \"Windows style. Type unix to come back.\") }\n    else if cmd == \"pwd\" { push(out, win[\"s\"][\"cwd\"]) }\n    else if cmd == \"ls\" {\n        ton.let names = \"\"\n        for e in atome_list(atome_sh_path(win, arg)) { names = names + e[\"name\"] + (e[\"dir\"] ? \"/\" : \"\") + \"  \" }\n        push(out, names == \"\" ? \"(empty)\" : names)\n    } else if cmd == \"cd\" {\n        ton.let p = atome_sh_path(win, arg == \"\" ? \"/home\" : arg)\n        if atome_is_dir(p) { win[\"s\"][\"cwd\"] = p } else { push(out, \"cd: no such folder: \" + arg) }\n    } else if cmd == \"cat\" {\n        ton.let t = atome_read(atome_sh_path(win, arg))\n        if t == nil { push(out, \"cat: no such file: \" + arg) } else { for l in split(t, \"\\n\") { push(out, l) } }\n    } else if cmd == \"mkdir\" { if !atome_mkdir(atome_sh_path(win, arg)) { push(out, \"mkdir: can't create \" + arg) } }\n    else if cmd == \"touch\" { if !atome_write(atome_sh_path(win, arg), atome_read(atome_sh_path(win, arg)) == nil ? \"\" : atome_read(atome_sh_path(win, arg))) { push(out, \"touch: can't create \" + arg) } }\n    else if cmd == \"rm\" { if !atome_remove(atome_sh_path(win, arg)) { push(out, \"rm: no such file: \" + arg) } }\n    else if cmd == \"echo\" { push(out, arg) }\n    else if cmd == \"clear\" { win[\"s\"][\"out\"] = [] }\n    else if cmd == \"apps\" { push(out, join(atome_os[\"order\"], \" \")) }\n    else if cmd == \"ps\" { for w in atome_os[\"wins\"] { push(out, w[\"id\"] + \"  \" + w[\"title\"]) } }\n    else if cmd == \"kill\" { if atome_find(arg) == nil { push(out, \"kill: no such process: \" + arg) } else { atome_close(arg) } }\n    else if cmd == \"open\" { if atome_open(arg) == nil { push(out, \"open: no app called \" + arg) } }\n    else if cmd == \"uname\" { push(out, atome_os[\"title\"] + \" (Atome, \" + atome_platform + \")\") }\n    else if cmd == \"time\" { push(out, atome_time()) }\n    else if cmd == \"neofetch\" {\n        push(out, atome_os[\"title\"])\n        push(out, \"platform: \" + atome_platform)\n        push(out, \"apps: \" + str(len(atome_os[\"order\"])) + \"   windows: \" + str(len(atome_os[\"wins\"])))\n        push(out, \"written in TON\")\n    } else { push(out, cmd + \": command not found (try help)\") }\n    if len(win[\"s\"][\"out\"]) > 40 { win[\"s\"][\"out\"] = win[\"s\"][\"out\"][len(win[\"s\"][\"out\"]) - 40:len(win[\"s\"][\"out\"])] }\n}\nton.function atome_term_init(win) {\n    win[\"s\"][\"cwd\"] = \"/home\"\n    win[\"s\"][\"mode\"] = atome_os[\"term\"]\n    win[\"s\"][\"out\"] = atome_os[\"term\"] == \"windows\" ? [\"Atome [Version 1.0]\", \"Type help for the commands.\"] : [\"Atome terminal. Type help.\"]\n}\nton.function atome_term_view(win) {\n    ton.let w = []\n    ton.let lines = win[\"s\"][\"out\"]\n    ton.let start = len(lines) > 9 ? len(lines) - 9 : 0\n    ton.let i = start\n    while i < len(lines) {\n        push(w, atome_label(lines[i]))\n        i = i + 1\n    }\n    push(w, atome_input(\"cmd\", win[\"s\"][\"mode\"] == \"windows\" ? atome_winpath(win[\"s\"][\"cwd\"]) + \">\" : win[\"s\"][\"cwd\"] + \" $\"))\n    return w\n}\nton.function atome_term_act(win, action, value) {\n    if action == \"enter:cmd\" {\n        ton.let line = atome_in(win, \"cmd\")\n        atome_set_in(win, \"cmd\", \"\")\n        if win[\"s\"][\"mode\"] == \"windows\" { atome_cmd(win, line) } else { atome_sh(win, line) }\n    }\n}\n\n// ---- Clock: the time, and a stopwatch that keeps counting while you use other apps (cooperative multitasking)\nton.function atome_clock_init(win) {\n    win[\"s\"][\"run\"] = false\n    win[\"s\"][\"ms\"] = 0\n    win[\"s\"][\"last\"] = 0\n}\nton.function atome_clock_total(win) { return win[\"s\"][\"ms\"] + (win[\"s\"][\"run\"] ? atome_now() - win[\"s\"][\"last\"] : 0) }\nton.function atome_clock_view(win) {\n    ton.let secs = (atome_clock_total(win) - atome_clock_total(win) % 1000) / 1000\n    ton.let m = (secs - secs % 60) / 60\n    return [atome_title(\"Clock\"), atome_title(atome_time()), atome_rule(), atome_label(\"Stopwatch  \" + atome_pad2(m) + \":\" + atome_pad2(secs % 60)),\n        atome_button(win[\"s\"][\"run\"] ? \"Pause\" : \"Start\", \"toggle\"), atome_button(\"Reset\", \"reset\"), atome_gap(),\n        atome_label(\"It keeps counting while you use the other apps.\")]\n}\nton.function atome_clock_tick(win) { return nil }\nton.function atome_clock_act(win, action, value) {\n    if action == \"toggle\" {\n        if win[\"s\"][\"run\"] { win[\"s\"][\"ms\"] = atome_clock_total(win)\n            win[\"s\"][\"run\"] = false }\n        else { win[\"s\"][\"last\"] = atome_now()\n            win[\"s\"][\"run\"] = true }\n    } else if action == \"reset\" { win[\"s\"][\"ms\"] = 0\n        win[\"s\"][\"last\"] = atome_now() }\n}\n\n// ---- Settings\nton.function atome_settings_view(win) {\n    ton.let w = [atome_title(\"Settings\"), atome_button(\"Change the colours\", \"wall\")]\n    if atome_os[\"style\"] != \"custom\" { push(w, atome_button(atome_os[\"style\"] == \"phone\" ? \"Switch to the Windows style\" : \"Switch to the Android style\", \"style\")) }\n    push(w, atome_button(\"Terminal style: \" + (atome_os[\"term\"] == \"windows\" ? \"Windows (cmd)\" : \"Unix\") + \" (change)\", \"term\"))\n    push(w, atome_rule())\n    push(w, atome_label(atome_os[\"title\"] + \" - running on \" + atome_platform))\n    push(w, atome_label(str(len(atome_os[\"order\"])) + \" apps, written in TON with Atome\"))\n    push(w, atome_rule())\n    push(w, atome_button(\"Reset the saved disk\", \"reset\"))\n    push(w, atome_button(\"Power off\", \"poweroff\"))\n    push(w, atome_button(\"Reboot\", \"reboot\"))\n    return w\n}\nton.function atome_settings_act(win, action, value) {\n    if action == \"wall\" { atome_os[\"wall\"] = (atome_os[\"wall\"] + 1) % 6 }\n    else if action == \"style\" { atome_os[\"style\"] = atome_os[\"style\"] == \"phone\" ? \"desktop\" : \"phone\"\n        atome_os[\"home\"] = true }\n    else if action == \"term\" { atome_os[\"term\"] = atome_os[\"term\"] == \"windows\" ? \"unix\" : \"windows\" }\n    else if action == \"reset\" {\n        atome_dirs = {\"/\": true, \"/home\": true, \"/home/docs\": true, \"/tmp\": true}\n        atome_files = {\"/home/readme.txt\": \"Welcome back!\"}\n        atome_save()\n        atome_toast(\"The disk was reset\")\n    } else if action == \"poweroff\" {\n        if atome_platform == \"kernel\" { poweroff() } else { atome_toast(\"Close the Run window to power off\") }\n    } else if action == \"reboot\" {\n        if atome_platform == \"kernel\" { reboot() }\n        else {\n            atome_os[\"wins\"] = []\n            atome_os[\"focus\"] = \"\"\n            atome_os[\"home\"] = true\n            atome_os[\"menu\"] = false\n            atome_toast(\"Rebooted\")\n        }\n    }\n}\nton.function atome_install_defaults() {\n    atome_app({\"id\": \"notes\", \"title\": \"Notes\", \"icon\": \"N\", \"view\": atome_notes_view, \"init\": atome_notes_init, \"act\": atome_notes_act, \"w\": 44, \"h\": 14})\n    atome_app({\"id\": \"calc\", \"title\": \"Calculator\", \"icon\": \"=\", \"view\": atome_calc_view, \"init\": atome_calc_init, \"act\": atome_calc_act, \"w\": 40, \"h\": 14})\n    atome_app({\"id\": \"files\", \"title\": \"Files\", \"icon\": \"F\", \"view\": atome_files_view, \"init\": atome_files_init, \"act\": atome_files_act, \"w\": 46, \"h\": 15})\n    atome_app({\"id\": \"terminal\", \"title\": \"Terminal\", \"icon\": \">\", \"view\": atome_term_view, \"init\": atome_term_init, \"act\": atome_term_act, \"w\": 56, \"h\": 15})\n    atome_app({\"id\": \"clock\", \"title\": \"Clock\", \"icon\": \"C\", \"view\": atome_clock_view, \"init\": atome_clock_init, \"act\": atome_clock_act, \"tick\": atome_clock_tick, \"w\": 40, \"h\": 13})\n    atome_app({\"id\": \"settings\", \"title\": \"Settings\", \"icon\": \"S\", \"view\": atome_settings_view, \"act\": atome_settings_act, \"w\": 44, \"h\": 15})\n    return nil\n}\n\n// ------------------------------------------------------------------ start\n// atome.style(\"windows\"), atome.style(\"android\"), atome.style(\"custom\") (your app fills the screen, the system adds\n// nothing) or atome.style(\"auto\") (Android on a narrow screen, Windows on a wide one). \"desktop\" and \"phone\" still work.\nton.function atome_style(name) {\n    if name == \"auto\" { atome_os[\"auto\"] = true\n        atome_os[\"style\"] = \"desktop\" }\n    else {\n        atome_os[\"auto\"] = false\n        atome_os[\"style\"] = name == \"windows\" ? \"desktop\" : (name == \"android\" ? \"phone\" : name)\n    }\n}\n// atome.terminal(\"windows\") or atome.terminal(\"unix\"): the style of new Terminal windows\nton.function atome_terminal(name) { atome_os[\"term\"] = name }\nton.function atome_run() {\n    if atome_platform == \"web\" {\n        ui_title(atome_os[\"title\"])\n        ui_on(\"event\", atome_web_event)\n        ui_every(1000, atome_web_tick)\n        atome_web_draw()\n    } else {\n        atome_k_run()\n    }\n}\nton.function atome_boot(title) {\n    atome_os[\"title\"] = title\n    atome_load()\n    atome_install_defaults()\n    return nil\n}\nton.let atome = {\"serialize\": atome_serialize, \"deserialize\": atome_deserialize, \"terminal\": atome_terminal, \"style\": atome_style, \"boot\": atome_boot, \"run\": atome_run, \"app\": atome_app, \"open\": atome_open, \"close\": atome_close, \"toast\": atome_toast,\n    \"label\": atome_label, \"title\": atome_title, \"button\": atome_button, \"item\": atome_item, \"input\": atome_input, \"gap\": atome_gap, \"rule\": atome_rule,\n    \"value\": atome_in, \"set_value\": atome_set_in, \"read\": atome_read, \"write\": atome_write, \"mkdir\": atome_mkdir, \"remove\": atome_remove,\n    \"list\": atome_list, \"exists\": atome_exists, \"is_dir\": atome_is_dir, \"join\": atome_join, \"parent\": atome_parent}\n";
// </atome-module>

class Interpreter {
  constructor(onPrint, files, uiOptions) {
    this.onPrint = onPrint || (() => {});
    this.uiOptions = uiOptions || {};
    // Optional project files ({"utils.ton": "...", "modules/x.ton": "..."})
    // so IMPORT://name works without a filesystem (the IDE, Acode plugin).
    this.files = files || null;
    this.globals = new Environment();
    this.importedModules = new Set();
    this.callDepth = 0;
    this.defineNatives();
    this.defineNativesV2();
    instanceToString = (inst) => {
      const m = inst.klass.findMethod("toString");
      if (!m) return undefined;
      return toDisplayString(this.callFunction(m, [], 0, inst));
    };
  }

  run(source) {
    const tokens = lex(source);
    const program = parse(tokens);
    for (const stmt of program) this.execute(stmt, this.globals);
  }

  error(line, msg) { throw new Error(`Error at line ${line ?? "?"}: ${msg}`); }

  checkType(kind, v, name, line, typeName_, env) {
    if (!kind || kind === "any") return;
    if (kind === "class") {
      let klass;
      try { klass = env.get(typeName_); } catch (e) { this.error(line, `Unknown type '${typeName_}' for 'ton.${name}'.`); }
      if (!(klass instanceof TonClass)) this.error(line, `'${typeName_}' is not a class, so it can't be used as the type of 'ton.${name}'.`);
      if (v === null) return;
      if (!(v instanceof TonInstance) || !v.klass.isSubclassOf(klass)) this.error(line, `Invalid type for 'ton.${name}': expected ${typeName_} but got ${typeName(v)}.`);
      return;
    }
    const checks = {
      int: () => typeof v === "number",
      float: () => typeof v === "number",
      string: () => typeof v === "string",
      bool: () => typeof v === "boolean",
      array: () => Array.isArray(v),
      dict: () => v instanceof Map,
      html: () => v instanceof HtmlString || typeof v === "string",
    };
    const expected = { int: "int/float", float: "int/float", string: "string", bool: "bool", array: "array", dict: "dict", html: "html" }[kind];
    if (!checks[kind]()) this.error(line, `Invalid type for 'ton.${name}': expected ${expected} but got ${typeName(v)}.`);
  }

  execute(stmt, env) {
    switch (stmt.kind) {
      case "ExprStmt": this.evaluate(stmt.expr, env); return;

      case "VarDecl": {
        let val = stmt.init ? this.evaluate(stmt.init, env) : null;
        if (stmt.pattern) { this.destructure(stmt, val, env); return; }
        this.checkType(stmt.declaredType, val, stmt.name, stmt.line, stmt.typeName, env);
        if (stmt.declaredType === "html" && typeof val === "string") val = new HtmlString(val);
        env.define(stmt.name, val, !!stmt.isConst);
        return;
      }

      case "ClassDecl": {
        let superclass = null;
        if (stmt.superName) {
          superclass = env.get(stmt.superName);
          if (!(superclass instanceof TonClass)) this.error(stmt.line, `'${stmt.superName}' is not a class (it's a ${typeName(superclass)}), so '${stmt.name}' can't extend it.`);
        }
        const methods = new Map(), fieldInits = [];
        const klass = new TonClass(stmt.name, superclass, methods, fieldInits, env);
        for (const m of stmt.members) {
          if (m.kind === "VarDecl") { fieldInits.push(m); continue; }
          const fn = new TonFunction(stmt.name + "." + m.name, m.params, m.body, env, m.paramDefaults, m.hasRest);
          fn.ownerClass = klass;
          fn.isInitializer = m.name === "init";
          methods.set(m.name, fn);
        }
        env.define(stmt.name, klass);
        return;
      }

      case "Block": {
        const blockEnv = new Environment(env);
        for (const s of stmt.statements) this.execute(s, blockEnv);
        return;
      }

      case "If": {
        if (isTruthy(this.evaluate(stmt.cond, env))) this.execute(stmt.thenBranch, env);
        else if (stmt.elseBranch) this.execute(stmt.elseBranch, env);
        return;
      }

      case "While": {
        while (isTruthy(this.evaluate(stmt.cond, env))) {
          try { this.execute(stmt.body, env); }
          catch (e) { if (e instanceof BreakSignal) break; if (e instanceof ContinueSignal) continue; throw e; }
        }
        return;
      }

      case "For": {
        const forEnv = new Environment(env);
        if (stmt.init) this.execute(stmt.init, forEnv);
        while (!stmt.cond || isTruthy(this.evaluate(stmt.cond, forEnv))) {
          try { this.execute(stmt.body, forEnv); }
          catch (e) {
            if (e instanceof BreakSignal) break;
            if (!(e instanceof ContinueSignal)) throw e;
          }
          if (stmt.incr) this.evaluate(stmt.incr, forEnv);
        }
        return;
      }

      case "ForIn": {
        // Each iteration gets its own scope (closures capture that iteration's value).
        const body = (a, b) => {
          const e = new Environment(env);
          if (stmt.name2 === null || stmt.name2 === undefined) e.define(stmt.name, b);
          else { e.define(stmt.name, a); e.define(stmt.name2, b); }
          this.execute(stmt.body, e);
        };
        const run = (a, b) => {
          try { body(a, b); return true; }
          catch (e) { if (e instanceof BreakSignal) return false; if (e instanceof ContinueSignal) return true; throw e; }
        };
        if (stmt.iterable.kind === "Range") {
          const lo = this.evaluate(stmt.iterable.left, env), hi = this.evaluate(stmt.iterable.right, env);
          if (typeof lo !== "number" || typeof hi !== "number") this.error(stmt.line, `A range needs two numbers (got ${typeName(lo)} and ${typeName(hi)}).`);
          const end = stmt.iterable.inclusive ? hi : hi - 1;
          let idx = 0;
          for (let i = lo; i <= end; i++, idx++) if (!run(idx, i)) break;
          return;
        }
        const collection = this.evaluate(stmt.iterable, env);
        if (Array.isArray(collection)) {
          for (let i = 0; i < collection.length; i++) if (!run(i, collection[i])) break;
        } else if (collection instanceof Map) {
          for (const [k, v] of [...collection.entries()]) if (!run(k, stmt.name2 ? v : k)) break;
        } else if (typeof collection === "string" || collection instanceof HtmlString) {
          const text = toDisplayString(collection);
          for (let i = 0; i < text.length; i++) if (!run(i, text[i])) break;
        } else {
          this.error(stmt.line, `'for ... in' expects an array, a dict, a string or a range, got ${typeName(collection)}.`);
        }
        return;
      }

      case "TryCatch": {
        try {
          try { this.execute(stmt.tryBlock, env); }
          catch (e) {
            if (e instanceof BreakSignal || e instanceof ContinueSignal || e instanceof ReturnSignal) throw e;
            if (!stmt.catchBlock) throw e; // no catch clause: propagate (finally still runs, below)
            const catchEnv = new Environment(env);
            catchEnv.define(stmt.errorVar, e instanceof ThrownValue ? e.value : (e instanceof RangeError ? "Error: Maximum recursion depth exceeded." : (e.message || String(e))));
            this.execute(stmt.catchBlock, catchEnv);
          }
        } catch (e) {
          if (stmt.finallyBlock) this.execute(stmt.finallyBlock, env);
          throw e;
        }
        if (stmt.finallyBlock) this.execute(stmt.finallyBlock, env);
        return;
      }

      case "Throw": throw new ThrownValue(stmt.value ? this.evaluate(stmt.value, env) : null);

      case "FnDecl": {
        env.define(stmt.name, new TonFunction(stmt.name, stmt.params, stmt.body, env, stmt.paramDefaults, stmt.hasRest));
        return;
      }

      case "Return": throw new ReturnSignal(stmt.value ? this.evaluate(stmt.value, env) : null);

      case "Print": this.onPrint(toDisplayString(this.evaluate(stmt.value, env))); return;

      case "Import":
        if (stmt.alias) env.define(stmt.alias, this.importNamespace(stmt.moduleName, stmt.line));
        else this.runImport(stmt.moduleName, stmt.line);
        return;

      case "Break": throw new BreakSignal();
      case "Continue": throw new ContinueSignal();

      case "Switch": {
        const subject = this.evaluate(stmt.subject, env);
        let matched = false;
        try {
          for (const { values, body } of stmt.cases) {
            const hit = values.some((v) => valuesEqual(subject, this.evaluate(v, env)));
            if (hit) { this.execute(body, new Environment(env)); matched = true; break; }
          }
          if (!matched && stmt.defaultBlock) this.execute(stmt.defaultBlock, new Environment(env));
        } catch (e) {
          if (!(e instanceof BreakSignal)) throw e;
          // A stray "break;" just exits the switch early, same as the real interpreter.
        }
        return;
      }

      default: throw new Error(`Unknown statement kind '${stmt.kind}'.`);
    }
  }

  evaluate(expr, env) {
    switch (expr.kind) {
      case "Literal": return expr.value;

      case "Variable":
        try { return env.get(expr.name); } catch (e) { this.error(expr.line, e.message); }
        break;

      case "Assign": {
        const val = this.evaluate(expr.value, env);
        const r = env.assign(expr.name, val);
        if (r === "const") this.error(expr.line, `Cannot reassign 'ton.${expr.name}': it was declared with ton.const.`);
        if (!r) {
          this.error(expr.line, `Cannot assign: 'ton.${expr.name}' was never declared. Declare it first with ton.let (or ton.int, ton.string, ...).`);
        }
        return val;
      }

      case "FString": return expr.parts.map((p) => toDisplayString(this.evaluate(p, env))).join("");

      case "Range": {
        const lo = this.evaluate(expr.left, env), hi = this.evaluate(expr.right, env);
        if (typeof lo !== "number" || typeof hi !== "number") this.error(expr.line, `A range needs two numbers (got ${typeName(lo)} and ${typeName(hi)}).`);
        const out = [], end = expr.inclusive ? hi : hi - 1;
        if (end - lo > 1e7) this.error(expr.line, "Range too large to build as an array (use it directly in a for loop instead).");
        for (let i = lo; i <= end; i++) out.push(i);
        return out;
      }

      case "Get": {
        const object = this.evaluate(expr.object, env);
        if (expr.optional && (object === null || object === undefined)) return null;
        if (!expr.value) return this.getProperty(object, expr.name, expr.line);
        const v = this.evaluate(expr.value, env);
        if (object instanceof TonInstance) { object.fields.set(expr.name, v); return v; }
        if (object instanceof Map) { object.set(expr.name, v); return v; }
        this.error(expr.line, `Cannot set property '${expr.name}' on ${typeName(object)}.`);
        break;
      }

      case "Super": {
        let self, owner;
        try { self = env.get("self"); owner = env.get("<class>"); } catch (e) { this.error(expr.line, "'super' can only be used inside a class method."); }
        if (!owner.superclass) this.error(expr.line, `Class '${owner.name}' has no parent class, so 'super' has nothing to call.`);
        const m = owner.superclass.findMethod(expr.name);
        if (!m) this.error(expr.line, `Parent class '${owner.superclass.name}' has no method '${expr.name}'.`);
        return this.bindMethod(m, self);
      }

      case "Slice": {
        const target = this.evaluate(expr.target, env);
        if (expr.optional && (target === null || target === undefined)) return null;
        const isText = typeof target === "string" || target instanceof HtmlString;
        if (!isText && !Array.isArray(target)) this.error(expr.line, `Only arrays and strings can be sliced (got ${typeName(target)}).`);
        const seq = isText ? toDisplayString(target) : target, n = seq.length;
        const bound = (e, d) => {
          if (!e) return d;
          let i = this.evaluate(e, env);
          if (typeof i !== "number") this.error(expr.line, `Slice bounds must be numbers (got ${typeName(i)}).`);
          i = Math.trunc(i);
          if (i < 0) i += n;
          return Math.max(0, Math.min(i, n));
        };
        const a = bound(expr.start, 0), b = Math.max(a, bound(expr.end, n));
        const out = seq.slice(a, b);
        return target instanceof HtmlString ? new HtmlString(out) : out;
      }

      case "FnExpr": return new TonFunction(expr.name, expr.params, expr.body, env, expr.paramDefaults, expr.hasRest);

      case "ArrayLit": {
        const out = [];
        for (const e of expr.elements) {
          if (e.kind === "Spread") {
            const spread = this.evaluate(e.expr, env);
            if (!Array.isArray(spread)) this.error(e.line, `'...' inside an array literal expects an array (got ${typeName(spread)}).`);
            out.push(...spread);
          } else {
            out.push(this.evaluate(e, env));
          }
        }
        return out;
      }

      case "DictLit": {
        const m = new Map();
        for (const [k, vExpr] of expr.entries) {
          if (k === null) { // spread entry: [null, {kind:"Spread", expr}]
            const spread = this.evaluate(vExpr.expr, env);
            if (!(spread instanceof Map)) this.error(vExpr.line, `'...' inside a dict literal expects a dict (got ${typeName(spread)}).`);
            for (const [sk, sv] of spread) m.set(sk, sv);
          } else {
            m.set(k, this.evaluate(vExpr, env));
          }
        }
        return m;
      }

      case "Ternary": return isTruthy(this.evaluate(expr.cond, env)) ? this.evaluate(expr.whenTrue, env) : this.evaluate(expr.whenFalse, env);

      case "Unary": {
        const v = this.evaluate(expr.operand, env);
        if (expr.op === "MINUS") {
          if (typeof v !== "number") this.error(expr.line, "Operand of '-' must be a number.");
          return -v;
        }
        if (expr.op === "TILDE") {
          if (typeof v !== "number") this.error(expr.line, "Operand of '~' must be a number.");
          return Number(~BigInt(Math.trunc(v)));
        }
        return !isTruthy(v);
      }

      case "Logical": {
        const left = this.evaluate(expr.left, env);
        if (expr.op === "or") { if (isTruthy(left)) return left; }
        else if (expr.op === "and") { if (!isTruthy(left)) return left; }
        else { if (left !== null && left !== undefined) return left; } // "??"
        return this.evaluate(expr.right, env);
      }

      case "Binary": return this.evalBinary(expr, env);

      case "Call": {
        const evalArgs = () => {
          const out = [];
          for (const a of expr.args) {
            if (a.kind === "Spread") {
              const v = this.evaluate(a.expr, env);
              if (!Array.isArray(v)) this.error(a.line, `'...' in a call expects an array (got ${typeName(v)}).`);
              out.push(...v);
            } else out.push(this.evaluate(a, env));
          }
          return out;
        };
        if (expr.callee.kind === "Get" && !expr.callee.value) return this.callMethod(expr.callee, evalArgs, expr.line, env);
        const callee = this.evaluate(expr.callee, env);
        if (expr.optional && (callee === null || callee === undefined)) return null;
        return this.callFunction(callee, evalArgs(), expr.line);
      }

      case "Index": {
        const target = this.evaluate(expr.target, env);
        if (expr.optional && (target === null || target === undefined)) return null;
        if (target instanceof TonInstance) {
          const key = toDisplayString(this.evaluate(expr.index, env));
          if (expr.value) { const v = this.evaluate(expr.value, env); target.fields.set(key, v); return v; }
          return target.fields.has(key) ? target.fields.get(key) : null;
        }
        if (target instanceof Map) {
          const keyVal = this.evaluate(expr.index, env);
          const key = typeof keyVal === "string" ? keyVal : toDisplayString(keyVal);
          if (expr.value) { const v = this.evaluate(expr.value, env); target.set(key, v); return v; }
          return target.has(key) ? target.get(key) : null;
        }
        if (typeof target === "string" || target instanceof HtmlString) {
          if (expr.value) this.error(expr.line, "Strings are immutable: cannot assign to a string index. Use replace()/substring() to build a new string instead.");
          const s = toDisplayString(target);
          const raw = Math.trunc(this.evaluate(expr.index, env));
          const idx = raw < 0 ? raw + s.length : raw;
          if (idx < 0 || idx >= s.length) this.error(expr.line, `String index ${raw} out of bounds (length ${s.length}).`);
          return s[idx];
        }
        if (!Array.isArray(target)) this.error(expr.line, `Cannot index a value that is not an array, a dict, or a string (got ${typeName(target)}).`);
        const raw = Math.trunc(this.evaluate(expr.index, env));
        const idx = raw < 0 ? raw + target.length : raw;
        if (expr.value) {
          const v = this.evaluate(expr.value, env);
          if (idx < 0 || idx >= target.length) this.error(expr.line, `Index ${raw} out of bounds (length ${target.length}).`);
          target[idx] = v;
          return v;
        }
        if (idx < 0 || idx >= target.length) this.error(expr.line, `Index ${raw} out of bounds (length ${target.length}).`);
        return target[idx];
      }

      default: throw new Error(`Unknown expression kind '${expr.kind}'.`);
    }
  }

  evalBinary(expr, env) {
    const left = this.evaluate(expr.left, env);
    const right = this.evaluate(expr.right, env);
    const op = expr.op;
    const isStrLike = (v) => typeof v === "string" || v instanceof HtmlString;
    const strOf = (v) => toDisplayString(v);

    switch (op) {
      case "PLUS":
        if (isStrLike(left) || isStrLike(right)) {
          const s = strOf(left) + strOf(right);
          return (left instanceof HtmlString || right instanceof HtmlString) ? new HtmlString(s) : s;
        }
        if (typeof left === "number" && typeof right === "number") return left + right;
        if (Array.isArray(left) && Array.isArray(right)) return [...left, ...right];
        this.error(expr.line, `Invalid operands for '+': ${typeName(left)} and ${typeName(right)}.`);
        break;
      case "STAR_STAR":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, `Invalid operands for '**': ${typeName(left)} and ${typeName(right)}.`);
        return Math.pow(left, right);
      case "MINUS":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, `Invalid operands for '-': ${typeName(left)} and ${typeName(right)}.`);
        return left - right;
      case "STAR":
        if (isStrLike(left) && typeof right === "number") {
          const r = strOf(left).repeat(Math.max(0, Math.trunc(right)));
          return left instanceof HtmlString ? new HtmlString(r) : r;
        }
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, `Invalid operands for '*': ${typeName(left)} and ${typeName(right)}.`);
        return left * right;
      case "SLASH":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, `Invalid operands for '/': ${typeName(left)} and ${typeName(right)}.`);
        if (right === 0) this.error(expr.line, "Division by zero.");
        return left / right;
      case "PERCENT":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, `Invalid operands for '%': ${typeName(left)} and ${typeName(right)}.`);
        if (right === 0) this.error(expr.line, "Division by zero (in '%').");
        return left % right;
      // Relational operators: numbers compare numerically, strings/html
      // lexicographically — mixing types (or comparing arrays/dicts/bools)
      // is a clear error, mirroring src/Interpreter.cpp's evaluate().
      case "GREATER": case "GREATER_EQUAL": case "LESS": case "LESS_EQUAL": {
        const bothNumbers = typeof left === "number" && typeof right === "number";
        const bothTextual = isStrLike(left) && isStrLike(right);
        if (!bothNumbers && !bothTextual) {
          this.error(expr.line, `Cannot compare ${typeName(left)} and ${typeName(right)} with a relational operator (<, <=, >, >=): both sides must be numbers, or both strings.`);
        }
        const a = bothNumbers ? left : strOf(left), b = bothNumbers ? right : strOf(right);
        if (op === "GREATER") return a > b;
        if (op === "GREATER_EQUAL") return a >= b;
        if (op === "LESS") return a < b;
        return a <= b;
      }
      case "EQUAL_EQUAL": return valuesEqual(left, right);
      case "BANG_EQUAL": return !valuesEqual(left, right);
      case "IN": {
        if (Array.isArray(right)) return right.some((item) => valuesEqual(item, left));
        if (right instanceof Map) {
          const key = typeof left === "string" ? left : strOf(left);
          return right.has(key);
        }
        if (isStrLike(right)) return strOf(right).includes(strOf(left));
        if (right instanceof TonInstance) return right.fields.has(strOf(left));
        this.error(expr.line, `'in' expects an array, dict, or string on the right-hand side (got ${typeName(right)}).`);
        break;
      }
      // Bitwise: same semantics as src/Interpreter.cpp — truncate both sides to
      // a 64-bit integer (via BigInt, since JS's native &|^<<>> are 32-bit
      // only) and convert the result back to a plain number.
      case "AMPERSAND": case "PIPE": case "CARET": case "LESS_LESS": case "GREATER_GREATER": {
        if (typeof left !== "number" || typeof right !== "number") {
          this.error(expr.line, `Bitwise operators need two numbers (got ${typeName(left)} and ${typeName(right)}).`);
        }
        const a = BigInt(Math.trunc(left)), b = BigInt(Math.trunc(right));
        if (op === "AMPERSAND") return Number(a & b);
        if (op === "PIPE") return Number(a | b);
        if (op === "CARET") return Number(a ^ b);
        if (op === "LESS_LESS") return Number(BigInt.asIntN(64, a << b));
        return Number(a >> b);
      }
      default: return null;
    }
  }

  bindMethod(method, self) {
    const env = new Environment(method.closure);
    env.define("self", self);
    if (method.ownerClass) env.define("<class>", method.ownerClass);
    const bound = new TonFunction(method.name, method.params, method.body, env, method.paramDefaults, method.hasRest);
    return bound;
  }

  getProperty(object, name, line) {
    if (object instanceof TonInstance) {
      if (object.fields.has(name)) return object.fields.get(name);
      const m = object.klass.findMethod(name);
      if (m) return this.bindMethod(m, object);
      this.error(line, `'${object.klass.name}' object has no property '${name}'.`);
    }
    if (object instanceof Map) return object.has(name) ? object.get(name) : null;
    if (object instanceof TonClass) {
      if (name === "name") return object.name;
      const m = object.findMethod(name);
      if (m) return m;
      this.error(line, `Class '${object.name}' has no method '${name}'.`);
    }
    if (object === null || object === undefined) this.error(line, `Cannot read property '${name}' of nil (use '?.' to get nil instead of an error).`);
    if (name === "length" && Array.isArray(object)) return object.length;
    if (name === "length" && (typeof object === "string" || object instanceof HtmlString)) return toDisplayString(object).length;
    this.error(line, `Cannot read property '${name}' of ${typeName(object)} (to call a function on it, write '.${name}()').`);
  }

  // obj.method(args) — methods, dict-stored functions, then uniform call
  // syntax: x.f(a) calls f(x, a). Mirrors Interpreter::callMethod.
  callMethod(getExpr, evalArgs, line, env) {
    const object = this.evaluate(getExpr.object, env);
    const name = getExpr.name;
    if (getExpr.optional && (object === null || object === undefined)) return null;
    if (object instanceof TonInstance) {
      if (object.fields.has(name)) return this.callFunction(object.fields.get(name), evalArgs(), line);
      const m = object.klass.findMethod(name);
      if (m) return this.callFunction(m, evalArgs(), line, object);
    } else if (object instanceof Map) {
      if (object.has(name)) return this.callFunction(object.get(name), evalArgs(), line);
    } else if (object instanceof TonClass) {
      const m = object.findMethod(name);
      if (m) return this.callFunction(m, evalArgs(), line);
      this.error(line, `Class '${object.name}' has no method '${name}'.`);
    } else if (object === null || object === undefined) {
      this.error(line, `Cannot call method '${name}' on nil (use '?.' to get nil instead of an error).`);
    }
    let fn = null;
    try { fn = env.get(name); } catch (e) { fn = null; }
    if (fn instanceof TonFunction || fn instanceof NativeFunction) return this.callFunction(fn, [object, ...evalArgs()], line);
    const owner = object instanceof TonInstance ? `'${object.klass.name}' object` : typeName(object);
    this.error(line, `${owner} has no method '${name}' (and there is no function '${name}' to call on it).`);
  }

  instantiate(klass, args, line) {
    const inst = new TonInstance(klass);
    const chain = [];
    for (let c = klass; c; c = c.superclass) chain.unshift(c);
    for (const c of chain) {
      if (!c.fieldInits.length) continue;
      const fieldEnv = new Environment(c.closure);
      fieldEnv.define("self", inst);
      for (const f of c.fieldInits) {
        let v = f.init ? this.evaluate(f.init, fieldEnv) : null;
        this.checkType(f.declaredType, v, f.name, f.line, f.typeName, fieldEnv);
        if (f.declaredType === "html" && typeof v === "string") v = new HtmlString(v);
        inst.fields.set(f.name, v);
      }
    }
    const init = klass.findMethod("init");
    if (init) this.callFunction(init, args, line, inst);
    else if (args.length) this.error(line, `'${klass.name}' has no init() method, so it takes no arguments (got ${args.length}).`);
    return inst;
  }

  destructure(stmt, val, env) {
    const p = stmt.pattern, c = !!stmt.isConst;
    if (p.isArray) {
      if (!Array.isArray(val)) this.error(stmt.line, `Cannot destructure ${typeName(val)} with an array pattern [...]: expected an array.`);
      p.names.forEach(([, name], i) => env.define(name, i < val.length ? val[i] : null, c));
      if (p.rest) env.define(p.rest, val.slice(p.names.length), c);
      return;
    }
    const src = val instanceof Map ? val : val instanceof TonInstance ? val.fields : null;
    if (!src) this.error(stmt.line, `Cannot destructure ${typeName(val)} with a dict pattern {...}: expected a dict or an object.`);
    for (const [key, name] of p.names) env.define(name, src.has(key) ? src.get(key) : null, c);
    if (p.rest) {
      const rest = new Map();
      for (const [k, v] of src) if (!p.names.some(([key]) => key === k)) rest.set(k, v);
      env.define(p.rest, rest, c);
    }
  }

  findModule(name) {
    if (this.files) for (const key of [name + ".ton", "modules/" + name + ".ton"]) if (key in this.files) return this.files[key];
    if (name === "atome") return ATOME_MODULE_SOURCE;   // built in: IMPORT://atome works everywhere the engine runs
    return null;
  }

  importNamespace(moduleName, line) {
    if (!moduleName.startsWith("ton.")) {
      const src = this.findModule(moduleName);
      if (src === null) this.runImport(moduleName, line); // reports "not found"
      const env = new Environment(this.globals);
      for (const stmt of parse(lex(src))) this.execute(stmt, env);
      return new Map(env.values);
    }
    const prefix = moduleName.slice(4) + "_";
    this.runImport(moduleName, line);
    const ns = new Map();
    for (const [k, v] of this.globals.values) if (k.startsWith(prefix)) ns.set(k.slice(prefix.length), v);
    return ns;
  }

  callFunction(callee, args, line, self = null) {
    if (callee instanceof NativeFunction) {
      try { return callee.fn(args); }
      catch (e) {
        if (e instanceof ThrownValue || e instanceof RangeError || /^Error at line/.test(e.message || "") || !line) throw e;
        this.error(line, e.message);
      }
    }
    if (callee instanceof TonClass) return this.instantiate(callee, args, line);
    if (!(callee instanceof TonFunction)) this.error(line, `Only functions and classes can be called (got ${typeName(callee)}).`);
    if (this.callDepth > 1500) this.error(line, `Maximum recursion depth (1500 in the playground) exceeded in '${callee.name}'.`);

    const hasRest = !!callee.hasRest;
    const fixedCount = hasRest ? callee.params.length - 1 : callee.params.length;
    const defaults = callee.paramDefaults || [];
    let requiredCount = 0;
    for (let i = 0; i < fixedCount; i++) if (!defaults[i]) requiredCount++;

    if (args.length < requiredCount || (!hasRest && args.length > fixedCount)) {
      let expectation;
      if (hasRest) expectation = `at least ${requiredCount}`;
      else if (requiredCount === fixedCount) expectation = `${fixedCount}`;
      else expectation = `between ${requiredCount} and ${fixedCount}`;
      this.error(line, `'ton.${callee.name || "<anonymous>"}' expects ${expectation} argument(s) but got ${args.length}.`);
    }

    const callEnv = new Environment(callee.closure);
    if (self !== null) {
      callEnv.define("self", self);
      if (callee.ownerClass) callEnv.define("<class>", callee.ownerClass);
    }
    for (let i = 0; i < fixedCount; i++) {
      const v = i < args.length ? args[i] : this.evaluate(defaults[i], callEnv);
      callEnv.define(callee.params[i], v);
    }
    if (hasRest) callEnv.define(callee.params[callee.params.length - 1], args.slice(fixedCount));

    this.callDepth++;
    try {
      for (const s of callee.body.statements) this.execute(s, callEnv);
    } catch (e) {
      if (e instanceof ReturnSignal) return callee.isInitializer && self ? self : e.value;
      throw e;
    } finally {
      this.callDepth--;
    }
    return callee.isInitializer && self ? self : null;
  }

  runImport(moduleName, line) {
    if (this.importedModules.has(moduleName)) return;
    this.importedModules.add(moduleName);

    if (moduleName.startsWith("ton.")) {
      const builtin = moduleName.slice(4);
      if (builtin === "random") return this.registerRandom();
      if (builtin === "time") return this.registerTime();
      if (builtin === "json") return this.registerJson();
      if (builtin === "mathutils") return this.registerMathUtils();
      if (builtin === "strings") return this.registerStrings();
      if (builtin === "requests") return this.registerRequests();
      if (builtin === "encoding") return this.registerEncoding();
      if (builtin === "ui") return this.registerUi();
      if (builtin === "sys" || builtin === "os") {
        this.error(line, `'ton.${builtin}' needs real process/filesystem/network access, which the browser playground cannot provide. Install the real interpreter (see DOCUMENTATION.md) to use it.`);
      }
      if (builtin === "regex" || builtin === "path" || builtin === "tensor") {
        this.error(line, `'ton.${builtin}' isn't implemented in the playground yet (it exists in the real interpreter — see DOCUMENTATION.md). Install the real interpreter to use it.`);
      }
      this.error(line, `Unknown built-in module 'ton.${builtin}'.`);
    }

    const src = this.findModule(moduleName);
    if (src !== null) {
      try {
        for (const stmt of parse(lex(src))) this.execute(stmt, this.globals);
      } catch (e) {
        if (e instanceof ThrownValue) throw e;
        this.error(line, `Error in module '${moduleName}': ${e.message}`);
      }
      return;
    }
    this.error(line, `IMPORT://${moduleName}: the playground has no filesystem, so it can't load user module files. This works in the real interpreter — see DOCUMENTATION.md > "Creating a TON618 module".`);
  }

  def(name, fn) { this.globals.define(name, new NativeFunction(name, fn)); }

  defineNatives() {
    this.def("print", (a) => { this.onPrint(toDisplayString(a[0])); return null; });
    this.def("platform", () => "web");   // the Atome kernel answers "kernel"
    this.def("type", (a) => typeName(a[0]));
    this.def("str", (a) => toDisplayString(a[0] ?? null));
    this.def("num", (a) => { const v = a[0]; if (typeof v === "number") return v; const n = parseFloat(v); return Number.isNaN(n) ? 0 : n; });
    this.def("format", (a) => {
      if (!a.length) return "";
      const tmpl = toDisplayString(a[0]);
      let out = "", argIndex = 1;
      for (let i = 0; i < tmpl.length; i++) {
        if (tmpl[i] === "{" && tmpl[i + 1] === "}") {
          out += argIndex < a.length ? toDisplayString(a[argIndex]) : "{}";
          argIndex++;
          i++;
          continue;
        }
        out += tmpl[i];
      }
      return out;
    });
    this.def("json", (a) => toJson(a[0] ?? null));
    this.def("assert", (a) => { if (!isTruthy(a[0])) throw new Error(a[1] !== undefined ? toDisplayString(a[1]) : "assertion failed"); return null; });

    this.def("len", (a) => {
      const v = a[0];
      if (typeof v === "string") return v.length;
      if (v instanceof HtmlString) return v.value.length;
      if (Array.isArray(v)) return v.length;
      if (v instanceof Map) return v.size;
      return 0;
    });

    // math
    this.def("sqrt", (a) => Math.sqrt(a[0] ?? 0));
    this.def("pow", (a) => Math.pow(a[0] ?? 0, a[1] ?? 0));
    this.def("abs", (a) => Math.abs(a[0] ?? 0));
    this.def("floor", (a) => Math.floor(a[0] ?? 0));
    this.def("ceil", (a) => Math.ceil(a[0] ?? 0));
    this.def("round", (a) => Math.round(a[0] ?? 0));
    this.def("min", (a) => Math.min(...a));
    this.def("max", (a) => Math.max(...a));
    this.def("random", (a) => {
      if (a.length === 0) return Math.random();
      if (a.length === 1) return Math.floor(Math.random() * Math.max(1, a[0]));
      const lo = a[0], hi = a[1];
      return hi <= lo ? lo : lo + Math.floor(Math.random() * (hi - lo));
    });

    // strings
    this.def("upper", (a) => toDisplayString(a[0]).toUpperCase());
    this.def("lower", (a) => toDisplayString(a[0]).toLowerCase());
    this.def("trim", (a) => toDisplayString(a[0]).trim());
    this.def("split", (a) => {
      const s = toDisplayString(a[0]);
      const sep = a[1] !== undefined ? toDisplayString(a[1]) : "";
      return sep === "" ? [s] : s.split(sep);
    });
    this.def("replace", (a) => toDisplayString(a[0]).split(toDisplayString(a[1])).join(toDisplayString(a[2] ?? "")));
    this.def("substring", (a) => {
      const s = toDisplayString(a[0]);
      const start = Math.max(0, Math.trunc(a[1] ?? 0));
      return a.length >= 3 ? s.substr(start, Math.max(0, Math.trunc(a[2]))) : s.slice(start);
    });
    this.def("contains", (a) => {
      if (Array.isArray(a[0])) return a[0].some((e) => valuesEqual(e, a[1]));
      return toDisplayString(a[0]).includes(toDisplayString(a[1]));
    });
    this.def("indexOf", (a) => {
      if (Array.isArray(a[0])) return a[0].findIndex((e) => valuesEqual(e, a[1]));
      const idx = toDisplayString(a[0]).indexOf(toDisplayString(a[1]));
      return idx;
    });

    // arrays
    this.def("push", (a) => { if (Array.isArray(a[0])) a[0].push(a[1]); return null; });
    this.def("pop", (a) => (Array.isArray(a[0]) && a[0].length ? a[0].pop() : null));
    this.def("shift", (a) => (Array.isArray(a[0]) && a[0].length ? a[0].shift() : null));
    this.def("unshift", (a) => { if (Array.isArray(a[0])) a[0].unshift(a[1]); return null; });
    this.def("slice", (a) => (Array.isArray(a[0]) ? a[0].slice(a[1] ?? 0, a[2]) : []));
    this.def("join", (a) => (Array.isArray(a[0]) ? a[0].map(toDisplayString).join(a[1] !== undefined ? toDisplayString(a[1]) : ",") : ""));
    this.def("sort", (a) => {
      if (!Array.isArray(a[0])) return a[0] ?? null;
      a[0].sort((x, y) => (typeof x === "number" && typeof y === "number") ? x - y : toDisplayString(x).localeCompare(toDisplayString(y)));
      return a[0];
    });
    this.def("reverse", (a) => { if (Array.isArray(a[0])) a[0].reverse(); return a[0] ?? null; });
    this.def("map", (a) => (Array.isArray(a[0]) ? a[0].map((item) => this.callFunction(a[1], [item], 0)) : []));
    this.def("filter", (a) => (Array.isArray(a[0]) ? a[0].filter((item) => isTruthy(this.callFunction(a[1], [item], 0))) : []));
    this.def("reduce", (a) => {
      if (!Array.isArray(a[0])) return null;
      let acc = a[2] !== undefined ? a[2] : null;
      for (const item of a[0]) acc = this.callFunction(a[1], [acc, item], 0);
      return acc;
    });
    this.def("find", (a) => {
      if (!Array.isArray(a[0])) return null;
      for (const item of a[0]) if (isTruthy(this.callFunction(a[1], [item], 0))) return item;
      return null;
    });
    this.def("any", (a) => {
      if (!Array.isArray(a[0])) return false;
      return a[0].some((item) => isTruthy(this.callFunction(a[1], [item], 0)));
    });
    this.def("all", (a) => {
      if (!Array.isArray(a[0])) return true;
      return a[0].every((item) => isTruthy(this.callFunction(a[1], [item], 0)));
    });

    // dicts
    this.def("keys", (a) => (a[0] instanceof Map ? [...a[0].keys()] : []));
    this.def("values", (a) => (a[0] instanceof Map ? [...a[0].values()] : []));
    this.def("has", (a) => (a[0] instanceof Map ? a[0].has(typeof a[1] === "string" ? a[1] : toDisplayString(a[1])) : false));

    // input() — the browser equivalent of reading a line from stdin.
    this.def("input", (a) => {
      const result = global.prompt ? global.prompt(a[0] !== undefined ? toDisplayString(a[0]) : "") : null;
      return result === null ? null : result;
    });
  }

  // The standard library added in beta-2.0.0 — mirrors src/Stdlib.cpp.
  defineNativesV2() {
    const fail = (m) => { throw new Error(m); };
    const isText = (v) => typeof v === "string" || v instanceof HtmlString;
    const callable = (v) => v instanceof TonFunction || v instanceof NativeFunction || v instanceof TonClass;
    const arr = (a, i, fn) => (Array.isArray(a[i]) ? a[i] : fail(`${fn}() expects an array as argument ${i + 1}${i < a.length ? ` (got ${typeName(a[i])})` : ""}.`));
    const text = (a, i, fn) => (isText(a[i]) ? toDisplayString(a[i]) : fail(`${fn}() expects a string as argument ${i + 1}${i < a.length ? ` (got ${typeName(a[i])})` : ""}.`));
    const num = (a, i, fn) => (typeof a[i] === "number" ? a[i] : fail(`${fn}() expects a number as argument ${i + 1}${i < a.length ? ` (got ${typeName(a[i])})` : ""}.`));
    const fnArg = (a, i, name) => (callable(a[i]) ? a[i] : fail(`${name}() expects a function as argument ${i + 1}.`));
    const less = (x, y) => (typeof x === "number" && typeof y === "number") ? x < y : toDisplayString(x) < toDisplayString(y);
    const cmp = (x, y) => (less(x, y) ? -1 : less(y, x) ? 1 : 0);
    const each = (fn, item, i) => this.callFunction(fn, (fn instanceof TonFunction && (fn.params.length >= 2 || fn.hasRest)) ? [item, i] : [item], 0);
    const fieldsOf = (v, fn) => (v instanceof Map ? v : v instanceof TonInstance ? v.fields : fail(`${fn}() expects a dict or an object (got ${typeName(v)}).`));
    const deep = (v) => {
      if (Array.isArray(v)) return v.map(deep);
      if (v instanceof Map) return new Map([...v].map(([k, x]) => [k, deep(x)]));
      if (v instanceof TonInstance) { const o = new TonInstance(v.klass); for (const [k, x] of v.fields) o.fields.set(k, deep(x)); return o; }
      return v;
    };
    const d = (name, fn) => this.def(name, fn);

    d("print", (a) => { this.onPrint(a.map(toDisplayString).join(" ")); return null; });
    d("int", (a) => {
      const v = a[0];
      if (typeof v === "number") return Math.trunc(v);
      if (typeof v === "boolean") return v ? 1 : 0;
      if (isText(v)) { const n = parseInt(toDisplayString(v), a.length >= 2 ? a[1] : 10); if (Number.isNaN(n)) fail(`int(): cannot convert "${toDisplayString(v)}" to an integer.`); return n; }
      fail(`int() cannot convert a ${typeName(v)}.`);
    });
    d("float", (a) => {
      const v = a[0];
      if (typeof v === "number") return v;
      if (typeof v === "boolean") return v ? 1 : 0;
      const n = Number(toDisplayString(v));
      if (!isText(v) || Number.isNaN(n)) fail(`float(): cannot convert "${toDisplayString(v)}" to a number.`);
      return n;
    });
    d("bool", (a) => isTruthy(a[0]));
    d("fixed", (a) => num(a, 0, "fixed").toFixed(a.length >= 2 ? num(a, 1, "fixed") : 2));
    d("repr", (a) => repr(a[0] ?? null));
    d("isinstance", (a) => {
      if (!(a[1] instanceof TonClass)) fail("isinstance(value, Class) expects a class as its second argument.");
      return a[0] instanceof TonInstance && a[0].klass.isSubclassOf(a[1]);
    });
    d("copy", (a) => {
      const v = a[0];
      if (Array.isArray(v)) return [...v];
      if (v instanceof Map) return new Map(v);
      if (v instanceof TonInstance) { const o = new TonInstance(v.klass); o.fields = new Map(v.fields); return o; }
      return v ?? null;
    });
    d("deepcopy", (a) => deep(a[0] ?? null));
    d("exit", () => { throw new Error("exit() called"); });
    const extreme = (a, wantMax, name) => {
      const items = a.length === 1 && Array.isArray(a[0]) ? a[0] : a;
      if (!items.length) fail(`${name}() of nothing: give at least one value.`);
      return items.reduce((best, v) => (wantMax ? less(best, v) : less(v, best)) ? v : best);
    };
    d("min", (a) => extreme(a, false, "min"));
    d("max", (a) => extreme(a, true, "max"));
    d("clamp", (a) => Math.max(num(a, 1, "clamp"), Math.min(num(a, 2, "clamp"), num(a, 0, "clamp"))));
    d("sign", (a) => Math.sign(num(a, 0, "sign")));
    d("round", (a) => {
      const x = num(a, 0, "round");
      if (a.length < 2) return Math.round(x);
      const f = Math.pow(10, num(a, 1, "round"));
      return Math.round(x * f) / f;
    });
    d("range", (a) => {
      let start = 0, end, step = 1;
      if (a.length === 1) end = num(a, 0, "range"); else { start = num(a, 0, "range"); end = num(a, 1, "range"); }
      if (a.length >= 3) step = num(a, 2, "range");
      if (step === 0) fail("range(): step can't be 0.");
      const out = [];
      if (step > 0) for (let i = start; i < end; i += step) out.push(i); else for (let i = start; i > end; i += step) out.push(i);
      return out;
    });
    d("sum", (a) => arr(a, 0, "sum").reduce((t, v) => (typeof v === "number" ? t + v : fail(`sum() expects an array of numbers (found a ${typeName(v)}).`)), 0));
    d("avg", (a) => { const x = arr(a, 0, "avg"); if (!x.length) fail("avg() of an empty array."); return x.reduce((t, v) => t + v, 0) / x.length; });
    d("len", (a) => {
      const v = a[0];
      if (isText(v)) return toDisplayString(v).length;
      if (Array.isArray(v)) return v.length;
      if (v instanceof Map) return v.size;
      if (v instanceof TonInstance) return v.fields.size;
      if (v === null || v === undefined) return 0;
      fail(`len() of a ${typeName(v)} makes no sense.`);
    });
    d("map", (a) => { const x = [...arr(a, 0, "map")], f = fnArg(a, 1, "map"); return x.map((v, i) => each(f, v, i)); });
    d("filter", (a) => { const x = [...arr(a, 0, "filter")], f = fnArg(a, 1, "filter"); return x.filter((v, i) => isTruthy(each(f, v, i))); });
    d("forEach", (a) => { const x = [...arr(a, 0, "forEach")], f = fnArg(a, 1, "forEach"); x.forEach((v, i) => each(f, v, i)); return null; });
    d("find", (a) => { const x = arr(a, 0, "find"), f = fnArg(a, 1, "find"); for (let i = 0; i < x.length; i++) if (isTruthy(each(f, x[i], i))) return x[i]; return null; });
    d("findIndex", (a) => { const x = arr(a, 0, "findIndex"), f = fnArg(a, 1, "findIndex"); for (let i = 0; i < x.length; i++) if (isTruthy(each(f, x[i], i))) return i; return -1; });
    d("any", (a) => { const x = arr(a, 0, "any"); if (a.length < 2) return x.some(isTruthy); const f = fnArg(a, 1, "any"); return x.some((v, i) => isTruthy(each(f, v, i))); });
    d("all", (a) => { const x = arr(a, 0, "all"); if (a.length < 2) return x.every(isTruthy); const f = fnArg(a, 1, "all"); return x.every((v, i) => isTruthy(each(f, v, i))); });
    d("reduce", (a) => {
      const x = arr(a, 0, "reduce"), f = fnArg(a, 1, "reduce");
      let i = 0, acc;
      if (a.length >= 3) acc = a[2]; else { if (!x.length) fail("reduce() of an empty array with no initial value."); acc = x[0]; i = 1; }
      for (; i < x.length; i++) acc = this.callFunction(f, [acc, x[i]], 0);
      return acc;
    });
    d("flatMap", (a) => { const x = arr(a, 0, "flatMap"), f = fnArg(a, 1, "flatMap"); return x.flatMap((v, i) => { const r = each(f, v, i); return Array.isArray(r) ? r : [r]; }); });
    d("count", (a) => {
      if (isText(a[0])) { const sub = toDisplayString(a[1]); if (!sub) fail("count(): the substring can't be empty."); return toDisplayString(a[0]).split(sub).length - 1; }
      const x = arr(a, 0, "count"), pred = a[1] instanceof TonFunction || a[1] instanceof NativeFunction;
      return x.filter((v, i) => (pred ? isTruthy(each(a[1], v, i)) : valuesEqual(v, a[1]))).length;
    });
    d("sort", (a) => {
      const x = arr(a, 0, "sort");
      if (a.length >= 2) {
        const sorted = [...x].sort((p, q) => { const r = this.callFunction(a[1], [p, q], 0); return typeof r === "number" ? r : (isTruthy(r) ? -1 : 1); });
        x.splice(0, x.length, ...sorted);
      } else x.sort(cmp);
      return x;
    });
    d("sorted", (a) => {
      const x = [...arr(a, 0, "sorted")];
      if (a.length >= 2) return x.map((v) => [this.callFunction(a[1], [v], 0), v]).sort((p, q) => cmp(p[0], q[0])).map((p) => p[1]);
      return x.sort(cmp);
    });
    d("reverse", (a) => {
      if (isText(a[0])) return [...toDisplayString(a[0])].reverse().join("");
      return arr(a, 0, "reverse").reverse();
    });
    d("unique", (a) => { const out = []; for (const v of arr(a, 0, "unique")) if (!out.some((o) => valuesEqual(o, v))) out.push(v); return out; });
    d("flatten", (a) => arr(a, 0, "flatten").flat(a.length >= 2 ? num(a, 1, "flatten") : 1));
    d("zip", (a) => { if (!a.length) return []; const n = Math.min(...a.map((x, i) => arr(a, i, "zip").length)); return Array.from({ length: n }, (_, r) => a.map((x) => x[r])); });
    d("enumerate", (a) => arr(a, 0, "enumerate").map((v, i) => [i, v]));
    d("chunk", (a) => { const x = arr(a, 0, "chunk"), n = num(a, 1, "chunk"); if (n < 1) fail("chunk(): the size must be at least 1."); const out = []; for (let i = 0; i < x.length; i += n) out.push(x.slice(i, i + n)); return out; });
    d("groupBy", (a) => { const m = new Map(); for (const v of arr(a, 0, "groupBy")) { const k = toDisplayString(this.callFunction(a[1], [v], 0)); if (!m.has(k)) m.set(k, []); m.get(k).push(v); } return m; });
    d("first", (a) => { const x = arr(a, 0, "first"); return x.length ? x[0] : null; });
    d("last", (a) => { const x = arr(a, 0, "last"); return x.length ? x[x.length - 1] : null; });
    d("insert", (a) => { const x = arr(a, 0, "insert"); let i = num(a, 1, "insert"); if (i < 0) i += x.length + 1; if (i < 0 || i > x.length) fail("insert(): index out of bounds."); x.splice(i, 0, a[2] ?? null); return x; });
    d("removeAt", (a) => { const x = arr(a, 0, "removeAt"); let i = num(a, 1, "removeAt"); if (i < 0) i += x.length; if (i < 0 || i >= x.length) fail("removeAt(): index out of bounds."); return x.splice(i, 1)[0]; });
    d("remove", (a) => {
      if (Array.isArray(a[0])) { const i = a[0].findIndex((v) => valuesEqual(v, a[1])); if (i < 0) return false; a[0].splice(i, 1); return true; }
      if (a[0] instanceof Map) return a[0].delete(toDisplayString(a[1]));
      fail("remove() expects an array or a dict.");
    });
    d("clear", (a) => { if (Array.isArray(a[0])) a[0].length = 0; else if (a[0] instanceof Map) a[0].clear(); else fail("clear() expects an array or a dict."); return a[0]; });
    d("isEmpty", (a) => { const v = a[0]; if (isText(v)) return !toDisplayString(v).length; if (Array.isArray(v)) return !v.length; if (v instanceof Map) return !v.size; return v === null || v === undefined; });
    d("keys", (a) => [...fieldsOf(a[0], "keys").keys()]);
    d("values", (a) => [...fieldsOf(a[0], "values").values()]);
    d("items", (a) => [...fieldsOf(a[0], "items").entries()].map(([k, v]) => [k, v]));
    d("has", (a) => fieldsOf(a[0], "has").has(toDisplayString(a[1])));
    d("dict", (a) => { const m = new Map(); if (a.length) for (const p of arr(a, 0, "dict")) { if (!Array.isArray(p) || p.length !== 2) fail("dict() expects an array of [key, value] pairs."); m.set(toDisplayString(p[0]), p[1]); } return m; });
    d("merge", (a) => { const m = new Map(); for (const x of a) for (const [k, v] of fieldsOf(x, "merge")) m.set(k, v); return m; });
    d("startsWith", (a) => text(a, 0, "startsWith").startsWith(toDisplayString(a[1] ?? "")));
    d("endsWith", (a) => text(a, 0, "endsWith").endsWith(toDisplayString(a[1] ?? "")));
    d("repeat", (a) => { const n = num(a, 1, "repeat"); if (Array.isArray(a[0])) return Array.from({ length: n }, () => a[0]).flat(); return text(a, 0, "repeat").repeat(n); });
    d("chars", (a) => [...text(a, 0, "chars")]);
    d("lines", (a) => { const t = text(a, 0, "lines"); const out = t.split("\n").map((l) => l.replace(/\r$/, "")); if (out.length && out[out.length - 1] === "") out.pop(); return out; });
    d("padStart", (a) => toDisplayString(a[0]).padStart(num(a, 1, "padStart"), a.length >= 3 ? toDisplayString(a[2]) : " "));
    d("padEnd", (a) => toDisplayString(a[0]).padEnd(num(a, 1, "padEnd"), a.length >= 3 ? toDisplayString(a[2]) : " "));
    d("capitalize", (a) => { const t = text(a, 0, "capitalize"); return t.charAt(0).toUpperCase() + t.slice(1); });
    d("lastIndexOf", (a) => { if (isText(a[0])) return toDisplayString(a[0]).lastIndexOf(toDisplayString(a[1])); const x = arr(a, 0, "lastIndexOf"); for (let i = x.length - 1; i >= 0; i--) if (valuesEqual(x[i], a[1])) return i; return -1; });
    d("ord", (a) => { const t = text(a, 0, "ord"); if (!t) fail("ord() of an empty string."); return t.charCodeAt(0); });
    d("chr", (a) => String.fromCharCode(num(a, 0, "chr")));
  }

  registerRandom() {
    this.def("random_int", (a) => { const lo = a[0], hi = a[1]; return hi <= lo ? lo : lo + Math.floor(Math.random() * (hi - lo + 1)); });
    this.def("random_float", () => Math.random());
    this.def("random_choice", (a) => (Array.isArray(a[0]) && a[0].length ? a[0][Math.floor(Math.random() * a[0].length)] : null));
    this.def("random_shuffle", (a) => {
      if (!Array.isArray(a[0])) return a[0] ?? null;
      const arr = a[0];
      for (let i = arr.length - 1; i > 0; i--) { const j = Math.floor(Math.random() * (i + 1));[arr[i], arr[j]] = [arr[j], arr[i]]; }
      return arr;
    });
    this.def("random_seed", () => null); // Math.random() can't be reseeded — accepted for script compatibility only.
  }

  registerTime() {
    this.def("time_now", () => Math.floor(Date.now() / 1000));
    this.def("time_millis", () => Date.now());
    this.def("time_string", (a) => new Date(a[0] !== undefined ? a[0] * 1000 : Date.now()).toString());
    this.def("time_sleep", () => null); // synchronous sleep isn't possible in JS; accepted as a no-op.
  }

  // ton.ui: an interactive screen (the module `tonos` is built on it). The program draws with
  // ui_render(html) and answers events registered with ui_on(type, fn); the host (Acode, the IDE,
  // the playground) shows the HTML in a sandboxed frame and sends clicks, typing and drags back.
  registerUi() {
    const ui = this.ui = { handlers: new Map(), timers: [], onUi: this.uiOptions.onUi || (() => {}), storage: Object.assign({}, this.uiOptions.storage || {}) };
    const text = (v) => toDisplayString(v ?? "");
    this.def("ui_render", (a) => { ui.onUi({ type: "render", html: text(a[0]) }); return null; });
    this.def("ui_on", (a) => { ui.handlers.set(text(a[0]), a[1]); return null; });
    this.def("ui_every", (a) => { ui.timers.push({ ms: Math.max(50, Number(a[0]) || 1000), fn: a[1] }); return null; });
    this.def("ui_title", (a) => { ui.onUi({ type: "title", text: text(a[0]) }); return null; });
    this.def("ui_storage_get", (a) => { const k = text(a[0]); return Object.prototype.hasOwnProperty.call(ui.storage, k) ? ui.storage[k] : (a[1] ?? null); });
    this.def("ui_storage_set", (a) => { const k = text(a[0]); ui.storage[k] = text(a[1]); ui.onUi({ type: "storage", key: k, value: ui.storage[k] }); return null; });
    this.def("ui_storage_remove", (a) => { const k = text(a[0]); delete ui.storage[k]; ui.onUi({ type: "storage", key: k, value: null }); return null; });
    this.def("ui_escape", (a) => text(a[0]).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;"));
    // the device's local time: {year, month, day, hour, minute, second, weekday (0 = Sunday)}
    this.def("ui_clock", () => {
      const d = new Date();
      return new Map([["year", d.getFullYear()], ["month", d.getMonth() + 1], ["day", d.getDate()], ["hour", d.getHours()], ["minute", d.getMinutes()], ["second", d.getSeconds()], ["weekday", d.getDay()]]);
    });
  }

  // ton.encoding: base64 and hex of a string's UTF-8 bytes (the real interpreter works on bytes too).
  registerEncoding() {
    const enc = new TextEncoder(), dec = new TextDecoder();
    const toHex = (bytes) => Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
    this.def("hex_encode", (a) => toHex(enc.encode(toDisplayString(a[0] ?? ""))));
    this.def("hex_decode", (a) => {
      const t = toDisplayString(a[0] ?? "");
      if (t.length % 2 || /[^0-9a-fA-F]/.test(t)) return "";
      return dec.decode(Uint8Array.from(t.match(/../g) || [], (h) => parseInt(h, 16)));
    });
    this.def("base64_encode", (a) => { let bin = ""; for (const b of enc.encode(toDisplayString(a[0] ?? ""))) bin += String.fromCharCode(b); return btoa(bin); });
    this.def("base64_decode", (a) => {
      try { const bin = atob(toDisplayString(a[0] ?? "")); return dec.decode(Uint8Array.from(bin, (c) => c.charCodeAt(0))); } catch (e) { return ""; }
    });
  }

  // ton.requests over a synchronous XMLHttpRequest (works in a Web Worker). Unlike the real
  // interpreter this is a browser request: the server must allow cross-origin requests (CORS).
  registerRequests() {
    const send = (method, url, headers, body, timeout) => {
      const out = new Map([["ok", false], ["status", 0], ["body", ""], ["error", ""], ["headers", new Map()], ["url", url]]);
      if (typeof XMLHttpRequest === "undefined") { out.set("error", "No network access in this environment."); return out; }
      try {
        const x = new XMLHttpRequest();
        x.open(String(method).toUpperCase(), url, false);
        if (headers instanceof Map) for (const [k, v] of headers) { try { x.setRequestHeader(k, toDisplayString(v)); } catch (e) { /* forbidden header: ignored */ } }
        if (timeout > 0) { try { x.timeout = timeout * 1000; } catch (e) { /* not allowed here */ } }
        x.send(body === null || body === undefined ? null : toDisplayString(body));
        out.set("ok", true); out.set("status", x.status); out.set("body", x.responseText || ""); out.set("url", x.responseURL || url);
        const hs = new Map();
        for (const line of (x.getAllResponseHeaders() || "").split(/\r?\n/)) { const i = line.indexOf(":"); if (i > 0) hs.set(line.slice(0, i).trim().toLowerCase(), line.slice(i + 1).trim()); }
        out.set("headers", hs);
      } catch (e) {
        out.set("error", `The request failed (${e.message || e}). From the browser or Acode the server must allow cross-origin requests (CORS).`);
      }
      return out;
    };
    this.def("requests_get", (a) => send("GET", toDisplayString(a[0]), null, null, 0));
    this.def("requests_post", (a) => send("POST", toDisplayString(a[0]), null, a[1] ?? null, 0));
    this.def("requests_request", (a) => send(toDisplayString(a[0]), toDisplayString(a[1]), null, a[2] ?? null, 0));
    this.def("requests_send", (a) => send(toDisplayString(a[0]), toDisplayString(a[1]), a[2] ?? null, a[3] ?? null, typeof a[4] === "number" ? a[4] : 0));
  }

  registerJson() {
    this.def("json_parse", (a) => fromJson(toDisplayString(a[0])));
    this.def("json_stringify", (a) => toJson(a[0] ?? null));
    this.def("json_pretty", (a) => toJsonPretty(a[0] ?? null));
  }

  // Mirrors src/Interpreter.cpp's registerBuiltinMathUtils exactly — every
  // function here is pure computation, so (unlike ton.sys/os/requests) there's
  // no reason for the playground not to support it in full.
  registerMathUtils() {
    this.def("mathutils_pi", () => Math.PI);
    this.def("mathutils_e", () => Math.E);
    this.def("mathutils_sin", (a) => Math.sin(a[0] ?? 0));
    this.def("mathutils_cos", (a) => Math.cos(a[0] ?? 0));
    this.def("mathutils_tan", (a) => Math.tan(a[0] ?? 0));
    this.def("mathutils_asin", (a) => Math.asin(a[0] ?? 0));
    this.def("mathutils_acos", (a) => Math.acos(a[0] ?? 0));
    this.def("mathutils_atan", (a) => Math.atan(a[0] ?? 0));
    this.def("mathutils_atan2", (a) => Math.atan2(a[0] ?? 0, a[1] ?? 0));
    this.def("mathutils_log", (a) => Math.log(a[0] ?? 0));
    this.def("mathutils_log2", (a) => Math.log2(a[0] ?? 0));
    this.def("mathutils_log10", (a) => Math.log10(a[0] ?? 0));
    this.def("mathutils_exp", (a) => Math.exp(a[0] ?? 0));
    this.def("mathutils_hypot", (a) => Math.hypot(a[0] ?? 0, a[1] ?? 0));
    this.def("mathutils_degrees", (a) => (a[0] ?? 0) * 180 / Math.PI);
    this.def("mathutils_radians", (a) => (a[0] ?? 0) * Math.PI / 180);
    this.def("mathutils_clamp", (a) => Math.min(Math.max(a[0], a[1]), a[2]));
    this.def("mathutils_lerp", (a) => a[0] + (a[1] - a[0]) * a[2]);
    this.def("mathutils_sign", (a) => Math.sign(a[0] ?? 0));
    this.def("mathutils_gcd", (a) => { let x = Math.abs(a[0] | 0), y = Math.abs(a[1] | 0); while (y) { [x, y] = [y, x % y]; } return x; });
    this.def("mathutils_lcm", (a) => {
      let x = Math.abs(a[0] | 0), y = Math.abs(a[1] | 0);
      if (x === 0 || y === 0) return 0;
      let g = x, h = y; while (h) { [g, h] = [h, g % h]; }
      return (x / g) * y;
    });
    this.def("mathutils_factorial", (a) => { let n = a[0] | 0, r = 1; for (let i = 2; i <= n; i++) r *= i; return r; });
    this.def("mathutils_is_prime", (a) => {
      let n = a[0] | 0;
      if (n < 2) return false;
      for (let i = 2; i * i <= n; i++) if (n % i === 0) return false;
      return true;
    });
    this.def("mathutils_sum", (a) => (Array.isArray(a[0]) ? a[0].reduce((s, v) => s + v, 0) : 0));
    this.def("mathutils_mean", (a) => (Array.isArray(a[0]) && a[0].length ? a[0].reduce((s, v) => s + v, 0) / a[0].length : 0));
    this.def("mathutils_median", (a) => {
      if (!Array.isArray(a[0]) || !a[0].length) return 0;
      const nums = [...a[0]].sort((x, y) => x - y);
      const n = nums.length;
      return n % 2 === 1 ? nums[(n - 1) / 2] : (nums[n / 2 - 1] + nums[n / 2]) / 2;
    });
    this.def("mathutils_stddev", (a) => {
      if (!Array.isArray(a[0]) || !a[0].length) return 0;
      const arr = a[0], mean = arr.reduce((s, v) => s + v, 0) / arr.length;
      const variance = arr.reduce((s, v) => s + (v - mean) ** 2, 0) / arr.length;
      return Math.sqrt(variance);
    });
    this.def("mathutils_min_of", (a) => (Array.isArray(a[0]) && a[0].length ? Math.min(...a[0]) : null));
    this.def("mathutils_max_of", (a) => (Array.isArray(a[0]) && a[0].length ? Math.max(...a[0]) : null));
  }

  // Mirrors src/Interpreter.cpp's registerBuiltinStrings exactly.
  registerStrings() {
    this.def("strings_starts_with", (a) => toDisplayString(a[0]).startsWith(toDisplayString(a[1] ?? "")));
    this.def("strings_ends_with", (a) => toDisplayString(a[0]).endsWith(toDisplayString(a[1] ?? "")));
    this.def("strings_repeat", (a) => toDisplayString(a[0]).repeat(Math.max(0, a[1] | 0)));
    this.def("strings_reverse", (a) => [...toDisplayString(a[0])].reverse().join(""));
    this.def("strings_capitalize", (a) => {
      const s = toDisplayString(a[0]);
      return s ? s[0].toUpperCase() + s.slice(1).toLowerCase() : "";
    });
    this.def("strings_pad_left", (a) => toDisplayString(a[0]).padStart(a[1] ?? 0, (a[2] !== undefined && toDisplayString(a[2])) || " "));
    this.def("strings_pad_right", (a) => toDisplayString(a[0]).padEnd(a[1] ?? 0, (a[2] !== undefined && toDisplayString(a[2])) || " "));
    this.def("strings_count", (a) => {
      const s = toDisplayString(a[0]), sub = toDisplayString(a[1] ?? "");
      if (!sub) return 0;
      let count = 0, pos = 0;
      while ((pos = s.indexOf(sub, pos)) !== -1) { count++; pos += sub.length; }
      return count;
    });
    this.def("strings_trim_start", (a) => toDisplayString(a[0]).replace(/^[ \t\n\r]+/, ""));
    this.def("strings_trim_end", (a) => toDisplayString(a[0]).replace(/[ \t\n\r]+$/, ""));
    this.def("strings_words", (a) => toDisplayString(a[0]).split(/\s+/).filter(Boolean));
    this.def("strings_center", (a) => {
      const s = toDisplayString(a[0]);
      const width = a[1] ?? 0;
      const ch = (a[2] !== undefined && toDisplayString(a[2])[0]) || " ";
      const total = width - s.length;
      if (total <= 0) return s;
      const left = Math.floor(total / 2), right = total - left;
      return ch.repeat(left) + s + ch.repeat(right);
    });
    this.def("strings_replace_first", (a) => {
      const s = toDisplayString(a[0]), search = toDisplayString(a[1] ?? ""), repl = toDisplayString(a[2] ?? "");
      if (!search) return s;
      const pos = s.indexOf(search);
      return pos === -1 ? s : s.slice(0, pos) + repl + s.slice(pos + search.length);
    });
    this.def("strings_snake_case", (a) => {
      const s = toDisplayString(a[0]);
      let out = "";
      for (let i = 0; i < s.length; i++) {
        const c = s[i];
        if (c === " " || c === "-") { out += "_"; continue; }
        if (/[A-Z]/.test(c) && i > 0 && !["_", " ", "-"].includes(s[i - 1])) out += "_";
        out += c.toLowerCase();
      }
      return out;
    });
    this.def("strings_camel_case", (a) => {
      const s = toDisplayString(a[0]);
      let out = "", upperNext = false;
      for (const c of s) {
        if (c === "_" || c === " " || c === "-") { upperNext = true; continue; }
        out += upperNext ? c.toUpperCase() : c;
        upperNext = false;
      }
      return out;
    });
  }
}

// Mirrors the real interpreter's valuesEqual (src/Interpreter.cpp): a string
// and an html value with the same text are equal; arrays/dicts compare
// element-by-element (recursively) instead of by reference.
function valuesEqual(a, b) {
  const aIsText = typeof a === "string" || a instanceof HtmlString;
  const bIsText = typeof b === "string" || b instanceof HtmlString;
  if (aIsText && bIsText) return toDisplayString(a) === toDisplayString(b);
  if (a === null || a === undefined) return b === null || b === undefined;
  if (Array.isArray(a) && Array.isArray(b)) {
    if (a.length !== b.length) return false;
    for (let i = 0; i < a.length; i++) if (!valuesEqual(a[i], b[i])) return false;
    return true;
  }
  if (a instanceof Map && b instanceof Map) {
    if (a.size !== b.size) return false;
    for (const [k, v] of a) { if (!b.has(k) || !valuesEqual(v, b.get(k))) return false; }
    return true;
  }
  if (typeof a !== typeof b) return false;
  return a === b; // classes and objects compare by identity
}

// Runs `source` and returns { ok, output } — output is every print() call's
// text joined with newlines; on failure ok is false and output ends with the
// error message, formatted like the real interpreter's stderr line.
// options: { files: {path: source} for IMPORT://, onPrint(text) to stream output }
function runTon618(source, options = {}) {
  const lines = [];
  try {
    const interp = new Interpreter((text) => { lines.push(text); if (options.onPrint) options.onPrint(text); }, options.files,
      { onUi: options.onUi, storage: options.storage });
    interp.run(source);
    const ui = interp.ui;
    // A program that registered ui handlers or timers stays alive: the host sends it events.
    const session = ui && (ui.handlers.size || ui.timers.length) ? makeSession(interp) : null;
    return { ok: true, output: lines.join("\n"), session };
  } catch (e) {
    lines.push(e.message || String(e));
    return { ok: false, output: lines.join("\n"), session: null };
  }
}

// What keeps an interactive program going after its last line: dispatch(event) calls the handler
// registered with ui_on for that event type (or the one for "event"), start() runs ui_every timers.
function makeSession(interp) {
  const ui = interp.ui, timers = [];
  const guard = (fn, args) => {
    try { interp.callFunction(fn, args, 0); }
    catch (e) { ui.onUi({ type: "error", text: e.message || (e && e.value !== undefined ? toDisplayString(e.value) : String(e)) }); }
  };
  return {
    dispatch(ev) {
      const h = ui.handlers.get(ev.type) || ui.handlers.get("event");
      if (h) guard(h, [jsToTon(ev)]);
    },
    start() { for (const t of ui.timers) timers.push(setInterval(() => guard(t.fn, []), t.ms)); },
    stop() { for (const t of timers.splice(0)) clearInterval(t); ui.handlers.clear(); },
  };
}

// ---------------------------------------------------------------------------
// The screen of an interactive program, for hosts with a DOM (the playground, the IDE, Acode).
// uiFrameHtml() is a small page that lives in a sandboxed iframe: it draws the HTML the program
// sends and reports clicks ([data-do]), typing ([data-bind], [data-key]), window drags
// ([data-drag]) and the screen size. mountUi(container, send) builds that iframe.
// ---------------------------------------------------------------------------
function uiFrameScript() {
  var root = document.getElementById("root");
  function send(ev) { parent.postMessage({ tonUi: true, event: ev }, "*"); }
  function attr(n, name) { return n.getAttribute(name) || ""; }
  function keepState(draw) {
    var a = document.activeElement, id = a && a.getAttribute && a.getAttribute("data-bind"), s = null, e = null;
    if (id) { s = a.selectionStart; e = a.selectionEnd; }
    var scrolls = [].map.call(root.querySelectorAll("[data-scroll]"), function (n) { return [attr(n, "data-scroll"), n.scrollTop]; });
    draw();
    scrolls.forEach(function (p) { var n = root.querySelector('[data-scroll="' + p[0] + '"]'); if (n) n.scrollTop = p[1]; });
    if (id) { var n = root.querySelector('[data-bind="' + id + '"]'); if (n) { n.focus(); try { n.setSelectionRange(s, e); } catch (x) { /* not a text field */ } } }
  }
  window.addEventListener("message", function (m) {
    var d = m.data;
    if (d && d.tonUi === "render") keepState(function () { root.innerHTML = d.html; });
  });
  root.addEventListener("click", function (e) {
    var t = e.target.closest("[data-do]");
    if (t) send({ type: "click", id: attr(t, "data-do"), win: attr(t, "data-win"), value: attr(t, "data-value") });
  });
  root.addEventListener("input", function (e) {
    var t = e.target;
    if (t.hasAttribute && t.hasAttribute("data-bind")) send({ type: "input", id: attr(t, "data-bind"), win: attr(t, "data-win"), value: t.value });
  });
  root.addEventListener("keydown", function (e) {
    var t = e.target;
    if (t.hasAttribute && t.hasAttribute("data-key")) {
      if (e.key === "Enter" && t.tagName !== "TEXTAREA") e.preventDefault();
      send({ type: "key", id: attr(t, "data-key"), win: attr(t, "data-win"), key: e.key, value: t.value });
    }
  });
  var drag = null;
  root.addEventListener("pointerdown", function (e) {
    var h = e.target.closest("[data-drag]");
    if (!h || e.target.closest("[data-do]")) return;
    var w = h.closest("[data-winbox]");
    if (!w) return;
    drag = { w: w, id: attr(h, "data-drag"), sx: e.clientX, sy: e.clientY, x: w.offsetLeft, y: w.offsetTop, moved: false };
    if (h.setPointerCapture) h.setPointerCapture(e.pointerId);
  });
  root.addEventListener("pointermove", function (e) {
    if (!drag) return;
    drag.moved = true;
    drag.w.style.left = Math.max(0, drag.x + e.clientX - drag.sx) + "px";
    drag.w.style.top = Math.max(0, drag.y + e.clientY - drag.sy) + "px";
  });
  function endDrag() {
    if (!drag) return;
    var d = drag; drag = null;
    send({ type: d.moved ? "move" : "click", id: d.moved ? "" : "focus", win: d.id, x: d.w.offsetLeft, y: d.w.offsetTop, value: "" });
  }
  root.addEventListener("pointerup", endDrag);
  root.addEventListener("pointercancel", endDrag);
  function size() { send({ type: "size", x: window.innerWidth, y: window.innerHeight, win: "", value: "" }); }
  window.addEventListener("resize", size);
  parent.postMessage({ tonUi: true, ready: true }, "*");
  size();
}

function uiFrameHtml() {
  return '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">' +
    '<style>html,body{margin:0;height:100%;overflow:hidden;background:#000;color:#fff;font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}' +
    '#root{position:absolute;top:0;right:0;bottom:0;left:0}input,textarea{user-select:text;-webkit-user-select:text;font:inherit}button{font:inherit}</style></head>' +
    '<body><div id="root"></div><script>(' + uiFrameScript.toString() + ')()<\/script></body></html>';
}

function mountUi(container, send) {
  const iframe = document.createElement("iframe");
  iframe.setAttribute("sandbox", "allow-scripts");
  iframe.setAttribute("title", "TON618 screen");
  iframe.style.cssText = "border:0;width:100%;height:100%;background:#000;display:block";
  iframe.srcdoc = uiFrameHtml();
  let ready = false, pending = null;
  const onMessage = (e) => {
    if (e.source !== iframe.contentWindow || !e.data || !e.data.tonUi) return;
    if (e.data.ready) { ready = true; if (pending) { iframe.contentWindow.postMessage(pending, "*"); pending = null; } return; }
    if (e.data.event) send(e.data.event);
  };
  window.addEventListener("message", onMessage);
  container.appendChild(iframe);
  return {
    render(html) { const m = { tonUi: "render", html }; if (ready) iframe.contentWindow.postMessage(m, "*"); else pending = m; },
    destroy() { window.removeEventListener("message", onMessage); iframe.remove(); },
  };
}

// Syntax check only: { ok: true } or { ok: false, line, message }.
function checkTon618(source) {
  try {
    parse(lex(source));
    return { ok: true };
  } catch (e) {
    const m = /[Ll]ine (\d+)/.exec(e.message || "");
    return { ok: false, line: m ? Number(m[1]) : 1, message: e.message || String(e) };
  }
}

global.Ton618Lite = { runTon618, checkTon618, uiFrameHtml, mountUi };

})(typeof window !== "undefined" ? window : globalThis);
