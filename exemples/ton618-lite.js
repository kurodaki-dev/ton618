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
// ton.random / ton.time / ton.json / ton.mathutils / ton.strings are
// re-implemented here in full, since none of those need anything a browser
// can't do.
//
// What it deliberately leaves out:
//   - ton.sys / ton.os / ton.requests (real process/filesystem/network access
//     — there's no sandbox equivalent to fall back to)
//   - ton.encoding / ton.regex / ton.path / ton.tensor (not reimplemented
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

class Interpreter {
  constructor(onPrint) {
    this.onPrint = onPrint || (() => {});
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

  importNamespace(moduleName, line) {
    if (!moduleName.startsWith("ton.")) this.runImport(moduleName, line); // errors: no filesystem
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
      if (builtin === "sys" || builtin === "os" || builtin === "requests") {
        this.error(line, `'ton.${builtin}' needs real process/filesystem/network access, which the browser playground cannot provide. Install the real interpreter (see DOCUMENTATION.md) to use it.`);
      }
      if (builtin === "encoding" || builtin === "regex" || builtin === "path" || builtin === "tensor") {
        this.error(line, `'ton.${builtin}' isn't implemented in the playground yet (it exists in the real interpreter — see DOCUMENTATION.md). Install the real interpreter to use it.`);
      }
      this.error(line, `Unknown built-in module 'ton.${builtin}'.`);
    }

    this.error(line, `IMPORT://${moduleName}: the playground has no filesystem, so it can't load user module files. This works in the real interpreter — see DOCUMENTATION.md > "Creating a TON618 module".`);
  }

  def(name, fn) { this.globals.define(name, new NativeFunction(name, fn)); }

  defineNatives() {
    this.def("print", (a) => { this.onPrint(toDisplayString(a[0])); return null; });
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
function runTon618(source) {
  const lines = [];
  try {
    const interp = new Interpreter((text) => lines.push(text));
    interp.run(source);
    return { ok: true, output: lines.join("\n") };
  } catch (e) {
    lines.push(e.message || String(e));
    return { ok: false, output: lines.join("\n") };
  }
}

global.Ton618Lite = { runTon618 };

})(typeof window !== "undefined" ? window : globalThis);
