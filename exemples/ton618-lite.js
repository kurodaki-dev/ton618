// ============================================================================
// ton618-lite.js — a JavaScript re-implementation of the TON618 language,
// used only by exemples/playground.html to run scripts directly in the
// browser (there's no WebAssembly build of the real C++ interpreter).
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
//   - ton.encoding / ton.regex / ton.path (not reimplemented here yet, though
//     nothing stops them from being — see the real interpreter's
//     registerBuiltinEncoding/Regex/Path in src/Interpreter.cpp)
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

    if (c === '"') {
      i++;
      // Mirrors the real interpreter's Lexer::string() (src/Lexer.cpp): plain
      // and simple on purpose — use the always-available format() native to
      // build a string out of variables instead of embedded expression syntax.
      let s = "";
      while (i < n && source[i] !== '"') {
        if (source[i] === "\\") {
          i++;
          const e = source[i++];
          s += { n: "\n", t: "\t", r: "\r", '"': '"', "\\": "\\" }[e] ?? e;
          continue;
        }
        if (source[i] === "\n") line++;
        s += source[i++];
      }
      i++; // closing quote
      push("STRING", s);
      continue;
    }

    if (isDigit(c)) {
      let start = i;
      while (isDigit(source[i])) i++;
      if (source[i] === "." && isDigit(peek(1))) { i++; while (isDigit(source[i])) i++; }
      push("NUMBER", parseFloat(source.slice(start, i)));
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

    const two = source.slice(i, i + 2);
    const twoMap = {
      "==": "EQUAL_EQUAL", "!=": "BANG_EQUAL", "<=": "LESS_EQUAL", ">=": "GREATER_EQUAL",
      "&&": "AND", "||": "OR", "+=": "PLUS_EQUAL", "-=": "MINUS_EQUAL", "*=": "STAR_EQUAL",
      "/=": "SLASH_EQUAL", "%=": "PERCENT_EQUAL", "++": "PLUS_PLUS", "--": "MINUS_MINUS",
      "??": "QUESTION_QUESTION",
    };
    if (twoMap[two]) { push(twoMap[two]); i += 2; continue; }

    const oneMap = {
      "(": "LPAREN", ")": "RPAREN", "{": "LBRACE", "}": "RBRACE", "[": "LBRACKET", "]": "RBRACKET",
      ",": "COMMA", ";": "SEMICOLON", ".": "DOT", "+": "PLUS", "-": "MINUS", "*": "STAR",
      "/": "SLASH", "%": "PERCENT", "=": "EQUAL", "!": "BANG", "<": "LESS", ">": "GREATER",
      "?": "QUESTION", ":": "COLON",
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
    pos -= 2; // rewind past TON DOT — this is a plain reference used as a statement
    return exprStatement();
  }

  function varDeclFromType(kind) {
    const line = previous().line;
    const name = consume("IDENTIFIER", "expected a variable name after the type.").value;
    consume("EQUAL", "expected '=': a ton.<type> declaration must have an initial value.");
    const init = expression();
    match("SEMICOLON");
    return { kind: "VarDecl", line, name, init, declaredType: kind };
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
        if (match("ELLIPSIS")) {
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
    }
    match("SEMICOLON");
    return { kind: "Import", line, moduleName };
  }

  function statement() {
    if (match("IF")) return ifStatement();
    if (match("WHILE")) return whileStatement();
    if (match("FOR")) return forStatement();
    if (match("TRY")) return tryStatement();
    if (match("THROW")) return throwStatement();
    if (match("SWITCH")) return switchStatement();
    if (match("RETURN")) return returnStatement();
    if (match("PRINT")) return printStatement();
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

    if (check("TON") && tokens[pos + 1].type === "DOT") {
      const saved = pos;
      advance(); advance(); // ton .
      if (check("IDENTIFIER")) {
        const itemName = advance().value;
        if (match("IN")) {
          const iterable = expression();
          if (paren) consume("RPAREN", "expected ')' after the for-in collection.");
          consume("LBRACE", "expected '{' for the for-in body.");
          const body = block();
          return { kind: "ForIn", line, name: itemName, iterable, body };
        }
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
      consume("LPAREN", "expected '(' after 'catch'.");
      if (check("TON")) { advance(); consume("DOT", "expected '.' after 'ton'."); errorVar = consume("IDENTIFIER", "expected an error variable name.").value; }
      else errorVar = consume("IDENTIFIER", "expected an error variable name.").value;
      consume("RPAREN", "expected ')' after the catch variable.");
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
    const value = expression();
    consume("RPAREN", "expected ')' after the print argument.");
    match("SEMICOLON");
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
    throw new Error(`Line ${line}: invalid assignment target.`);
  }

  const COMPOUND_OPS = { PLUS_EQUAL: "PLUS", MINUS_EQUAL: "MINUS", STAR_EQUAL: "STAR", SLASH_EQUAL: "SLASH", PERCENT_EQUAL: "PERCENT" };

  function assignment() {
    const expr = ternary();
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
    let expr = equality();
    while (match("AND")) { const line = previous().line; expr = { kind: "Logical", line, op: "and", left: expr, right: equality() }; }
    return expr;
  }
  const equality = binaryLevel(() => comparison(), ["EQUAL_EQUAL", "BANG_EQUAL"]);
  const comparison = binaryLevel(() => term(), ["LESS", "LESS_EQUAL", "GREATER", "GREATER_EQUAL", "IN"]);
  const term = binaryLevel(() => factor(), ["PLUS", "MINUS"]);
  const factor = binaryLevel(() => unary(), ["STAR", "SLASH", "PERCENT"]);

  function unary() {
    if (match("BANG", "MINUS")) {
      const opTok = previous();
      return { kind: "Unary", line: opTok.line, op: opTok.type, operand: unary() };
    }
    return call();
  }

  function call() {
    let expr = primary();
    while (true) {
      if (match("LPAREN")) {
        const line = previous().line;
        const args = [];
        if (!check("RPAREN")) { do { args.push(expression()); } while (match("COMMA")); }
        consume("RPAREN", "expected ')' after the arguments.");
        expr = { kind: "Call", line, callee: expr, args };
      } else if (match("LBRACKET")) {
        const line = previous().line;
        const index = expression();
        consume("RBRACKET", "expected ']' after the index.");
        expr = { kind: "Index", line, target: expr, index };
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
    if (check("TON")) return tonReference();
    if (match("IDENTIFIER")) return { kind: "Variable", line: previous().line, name: previous().value };
    if (match("LPAREN")) {
      const inner = expression();
      consume("RPAREN", "expected ')' after the expression.");
      return inner;
    }
    if (match("LBRACKET")) {
      const elements = [];
      if (!check("RBRACKET")) { do { elements.push(expression()); } while (match("COMMA")); }
      consume("RBRACKET", "expected ']' after the array elements.");
      return { kind: "ArrayLit", elements };
    }
    if (match("LBRACE")) {
      const entries = [];
      if (!check("RBRACE")) {
        do {
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

  return parseProgram();
}

// ---------------------------------------------------------------------------
// Values — plain JS values double as TON618 values, with two wrapper classes
// for the cases JS has no native equivalent for: HTML-tagged strings and dicts
// (kept as insertion-ordered Maps, matching the real interpreter's semantics).
// ---------------------------------------------------------------------------

class HtmlString { constructor(s) { this.value = s; } }
class TonFunction { constructor(name, params, body, closure, paramDefaults = [], hasRest = false) { Object.assign(this, { name, params, body, closure, paramDefaults, hasRest }); } }
class NativeFunction { constructor(name, fn) { this.name = name; this.fn = fn; } }

function typeName(v) {
  if (v === null || v === undefined) return "nil";
  if (typeof v === "number") return "int/float";
  if (typeof v === "string") return "string";
  if (typeof v === "boolean") return "bool";
  if (Array.isArray(v)) return "array";
  if (v instanceof Map) return "dict";
  if (v instanceof HtmlString) return "html";
  if (v instanceof TonFunction || v instanceof NativeFunction) return "function";
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
  if (typeof v === "number") return Number.isInteger(v) ? String(v) : String(v);
  if (typeof v === "string") return v;
  if (v instanceof HtmlString) return v.value;
  if (Array.isArray(v)) return "[" + v.map(toDisplayString).join(", ") + "]";
  if (v instanceof Map) return "{" + [...v.entries()].map(([k, val]) => `${k}: ${toDisplayString(val)}`).join(", ") + "}";
  if (v instanceof TonFunction) return `<ton.function ${v.name || ""}>`;
  if (v instanceof NativeFunction) return "<native function>";
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
  constructor(parent = null) { this.values = new Map(); this.parent = parent; }
  define(name, v) { this.values.set(name, v); }
  get(name) {
    if (this.values.has(name)) return this.values.get(name);
    if (this.parent) return this.parent.get(name);
    throw new Error(`Undefined variable or function 'ton.${name}'.`);
  }
  has(name) { return this.values.has(name) || (this.parent && this.parent.has(name)); }
  assign(name, v) {
    if (this.values.has(name)) { this.values.set(name, v); return true; }
    if (this.parent) return this.parent.assign(name, v);
    return false;
  }
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
    this.defineNatives();
  }

  run(source) {
    const tokens = lex(source);
    const program = parse(tokens);
    for (const stmt of program) this.execute(stmt, this.globals);
  }

  error(line, msg) { throw new Error(`Error at line ${line ?? "?"}: ${msg}`); }

  checkType(kind, v, name, line) {
    if (!kind) return;
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
        this.checkType(stmt.declaredType, val, stmt.name, stmt.line);
        if (stmt.declaredType === "html" && typeof val === "string") val = new HtmlString(val);
        env.define(stmt.name, val);
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
        const collection = this.evaluate(stmt.iterable, env);
        const forEnv = new Environment(env);
        let items;
        if (Array.isArray(collection)) items = collection;
        else if (collection instanceof Map) items = [...collection.keys()];
        else this.error(stmt.line, `'for ... in' expects an array or a dict, got ${typeName(collection)}.`);
        for (const item of items) {
          forEnv.define(stmt.name, item);
          try { this.execute(stmt.body, forEnv); }
          catch (e) { if (e instanceof BreakSignal) break; if (e instanceof ContinueSignal) continue; throw e; }
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
            catchEnv.define(stmt.errorVar, e.message || String(e));
            this.execute(stmt.catchBlock, catchEnv);
          }
        } catch (e) {
          if (stmt.finallyBlock) this.execute(stmt.finallyBlock, env);
          throw e;
        }
        if (stmt.finallyBlock) this.execute(stmt.finallyBlock, env);
        return;
      }

      case "Throw": throw new Error(toDisplayString(stmt.value ? this.evaluate(stmt.value, env) : null));

      case "FnDecl": {
        env.define(stmt.name, new TonFunction(stmt.name, stmt.params, stmt.body, env, stmt.paramDefaults, stmt.hasRest));
        return;
      }

      case "Return": throw new ReturnSignal(stmt.value ? this.evaluate(stmt.value, env) : null);

      case "Print": this.onPrint(toDisplayString(this.evaluate(stmt.value, env))); return;

      case "Import": this.runImport(stmt.moduleName, stmt.line); return;

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

      case "Variable": return env.get(expr.name);

      case "Assign": {
        const val = this.evaluate(expr.value, env);
        if (!env.assign(expr.name, val)) {
          this.error(expr.line, `Cannot assign: 'ton.${expr.name}' was never declared. Use ton.int/ton.string/ton.bool/ton.float/ton.array/ton.dict/ton.html to declare it first.`);
        }
        return val;
      }

      case "FnExpr": return new TonFunction(expr.name, expr.params, expr.body, env, expr.paramDefaults, expr.hasRest);

      case "ArrayLit": return expr.elements.map((e) => this.evaluate(e, env));

      case "DictLit": {
        const m = new Map();
        for (const [k, vExpr] of expr.entries) m.set(k, this.evaluate(vExpr, env));
        return m;
      }

      case "Ternary": return isTruthy(this.evaluate(expr.cond, env)) ? this.evaluate(expr.whenTrue, env) : this.evaluate(expr.whenFalse, env);

      case "Unary": {
        const v = this.evaluate(expr.operand, env);
        if (expr.op === "MINUS") {
          if (typeof v !== "number") this.error(expr.line, "Operand of '-' must be a number.");
          return -v;
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
        const callee = this.evaluate(expr.callee, env);
        const args = expr.args.map((a) => this.evaluate(a, env));
        return this.callFunction(callee, args, expr.line);
      }

      case "Index": {
        const target = this.evaluate(expr.target, env);
        if (target instanceof Map) {
          const keyVal = this.evaluate(expr.index, env);
          const key = typeof keyVal === "string" ? keyVal : toDisplayString(keyVal);
          if (expr.value) { const v = this.evaluate(expr.value, env); target.set(key, v); return v; }
          return target.has(key) ? target.get(key) : null;
        }
        if (typeof target === "string" || target instanceof HtmlString) {
          if (expr.value) this.error(expr.line, "Strings are immutable: cannot assign to a string index. Use replace()/substring() to build a new string instead.");
          const s = toDisplayString(target);
          const idx = Math.trunc(this.evaluate(expr.index, env));
          if (idx < 0 || idx >= s.length) this.error(expr.line, "String index out of bounds.");
          return s[idx];
        }
        if (!Array.isArray(target)) this.error(expr.line, `Cannot index a value that is not an array, a dict, or a string (got ${typeName(target)}).`);
        const idx = Math.trunc(this.evaluate(expr.index, env));
        if (expr.value) {
          const v = this.evaluate(expr.value, env);
          if (idx < 0 || idx >= target.length) this.error(expr.line, "Index out of bounds.");
          target[idx] = v;
          return v;
        }
        if (idx < 0 || idx >= target.length) this.error(expr.line, "Index out of bounds.");
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
        this.error(expr.line, "Invalid operands for '+'.");
        break;
      case "MINUS":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, "Invalid operands for '-'.");
        return left - right;
      case "STAR":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, "Invalid operands for '*'.");
        return left * right;
      case "SLASH":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, "Invalid operands for '/'.");
        if (right === 0) this.error(expr.line, "Division by zero.");
        return left / right;
      case "PERCENT":
        if (typeof left !== "number" || typeof right !== "number") this.error(expr.line, "Invalid operands for '%'.");
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
        this.error(expr.line, `'in' expects an array, dict, or string on the right-hand side (got ${typeName(right)}).`);
        break;
      }
      default: return null;
    }
  }

  callFunction(callee, args, line) {
    if (callee instanceof NativeFunction) return callee.fn(args);
    if (!(callee instanceof TonFunction)) this.error(line, "Only functions can be called.");

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
    for (let i = 0; i < fixedCount; i++) {
      const v = i < args.length ? args[i] : this.evaluate(defaults[i], callEnv);
      callEnv.define(callee.params[i], v);
    }
    if (hasRest) callEnv.define(callee.params[callee.params.length - 1], args.slice(fixedCount));

    try {
      for (const s of callee.body.statements) this.execute(s, callEnv);
    } catch (e) {
      if (e instanceof ReturnSignal) return e.value;
      throw e;
    }
    return null;
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
      if (builtin === "encoding" || builtin === "regex" || builtin === "path") {
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
  return a === b;
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
