#include "Parser.hpp"
#include <stdexcept>

Parser::Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

bool Parser::isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }
const Token& Parser::peek() const { return tokens[current]; }
const Token& Parser::peekNext() const {
    if (current + 1 >= (int)tokens.size()) return tokens.back();
    return tokens[current + 1];
}
const Token& Parser::previous() const { return tokens[current - 1]; }

const Token& Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return type == TokenType::END_OF_FILE;
    return peek().type == type;
}

bool Parser::checkNext(TokenType type) const {
    return peekNext().type == type;
}

bool Parser::match(std::vector<TokenType> types) {
    for (auto t : types) if (check(t)) { advance(); return true; }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    error(peek(), message);
}

void Parser::error(const Token& token, const std::string& message) {
    std::string where = (token.type == TokenType::END_OF_FILE) ? "end of file" : "'" + token.lexeme + "'";
    throw std::runtime_error("Ligne " + std::to_string(token.line) + " near " + where + ": " + message);
}

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> stmts;
    while (!isAtEnd()) stmts.push_back(declaration());
    return stmts;
}

StmtPtr Parser::declaration() {
    if (check(TokenType::TON)) return tonDeclaration();
    if (check(TokenType::IMPORT_ARROW)) return importDeclaration();
    return statement();
}

// "ton." peut introduire : un type (declaration de variable), "function" (declaration
// de fonction), ou juste un identifiant (utilisation en tant que statement, ex: ton.x = 5)
StmtPtr Parser::tonDeclaration() {
    advance(); // consomme TON
    consume(TokenType::DOT, "expected '.' after 'ton'.");

    if (check(TokenType::TYPE_INT))    { advance(); return varDeclFromType(VarKind::INT); }
    if (check(TokenType::TYPE_FLOAT))  { advance(); return varDeclFromType(VarKind::FLOAT); }
    if (check(TokenType::TYPE_STRING)) { advance(); return varDeclFromType(VarKind::STRING); }
    if (check(TokenType::TYPE_BOOL))   { advance(); return varDeclFromType(VarKind::BOOL); }
    if (check(TokenType::TYPE_ARRAY))  { advance(); return varDeclFromType(VarKind::ARRAY); }
    if (check(TokenType::TYPE_HTML))   { advance(); return varDeclFromType(VarKind::HTML); }
    if (check(TokenType::TYPE_DICT))   { advance(); return varDeclFromType(VarKind::DICT); }
    if (check(TokenType::FUNCTION))    { advance(); return fnDeclaration(); }

    // Sinon: c'est une utilisation de variable existante en debut de statement,
    // ex: "ton.x = 5" (reassignation) ou "ton.foo()" (appel en statement).
    // On revient en arriere pour laisser exprStatement/assignment gerer ca normalement.
    current -= 2; // recule avant TON et DOT
    return exprStatement();
}

StmtPtr Parser::varDeclFromType(VarKind kind) {
    int line = previous().line;
    Token name = consume(TokenType::IDENTIFIER, "expected a variable name after the type.");
    consume(TokenType::EQUAL, "expected '=': a ton.<type> declaration must have an initial value.");
    ExprPtr init = expression();
    match({TokenType::SEMICOLON});

    auto s = std::make_shared<Stmt>();
    s->type = StmtType::VAR_DECL;
    s->line = line;
    s->name = name.lexeme;
    s->initializer = init;
    s->declaredType = kind;
    return s;
}

StmtPtr Parser::fnDeclaration() {
    int line = previous().line;
    Token name = consume(TokenType::IDENTIFIER, "expected a function name.");
    consume(TokenType::LPAREN, "expected '(' after the function name.");
    ParamList params = parseParamList();
    consume(TokenType::RPAREN, "expected ')' after the parameters.");
    consume(TokenType::LBRACE, "expected '{' before the function body.");
    StmtPtr body = block();

    auto s = std::make_shared<Stmt>();
    s->type = StmtType::FN_DECL;
    s->line = line;
    s->fnName = name.lexeme;
    s->params = params.names;
    s->paramDefaults = params.defaults;
    s->hasRestParam = params.hasRest;
    s->body = body;
    return s;
}

// Parses "(a, b = default, ...rest)" (the opening '(' is already consumed by
// the caller) — shared by fnDeclaration() and functionExpression(). A default
// value expression can reference earlier parameters (they're bound in the
// call environment first — see Interpreter::callFunction). "...name" collects
// every remaining argument into a ton.array and, if present, must be last.
Parser::ParamList Parser::parseParamList() {
    ParamList result;
    if (!check(TokenType::RPAREN)) {
        do {
            if (match({TokenType::ELLIPSIS})) {
                Token restName = consume(TokenType::IDENTIFIER, "expected a parameter name after '...'.");
                result.names.push_back(restName.lexeme);
                result.defaults.push_back(nullptr);
                result.hasRest = true;
                break;
            }
            Token p = consume(TokenType::IDENTIFIER, "expected a parameter name.");
            ExprPtr def = nullptr;
            if (match({TokenType::EQUAL})) def = expression();
            result.names.push_back(p.lexeme);
            result.defaults.push_back(def);
        } while (match({TokenType::COMMA}));
    }
    return result;
}

// "IMPORT://name" loads a user-written module file (name.ton). "IMPORT://ton.name"
// instead loads one of the interpreter's built-in system modules (ton.sys, ton.os,
// ton.requests, ton.random, ton.time, ton.json — see Interpreter::runImport) — the
// leading "ton." is not a file lookup, it's how the parser tells the two apart.
StmtPtr Parser::importDeclaration() {
    int line = peek().line;
    advance(); // consomme IMPORT_ARROW ("IMPORT://")

    std::string moduleName;
    if (check(TokenType::TON)) {
        advance();
        consume(TokenType::DOT, "expected '.' after 'ton'.");
        Token name = consume(TokenType::IDENTIFIER, "expected a built-in module name after 'ton.'.");
        moduleName = "ton." + name.lexeme;
    } else {
        Token name = consume(TokenType::IDENTIFIER, "expected a module name after 'IMPORT://'.");
        moduleName = name.lexeme;
    }

    match({TokenType::SEMICOLON});
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::IMPORT;
    s->line = line;
    s->moduleName = moduleName;
    return s;
}

StmtPtr Parser::statement() {
    if (match({TokenType::IF})) return ifStatement();
    if (match({TokenType::WHILE})) return whileStatement();
    if (match({TokenType::FOR})) return forStatement();
    if (match({TokenType::TRY})) return tryStatement();
    if (match({TokenType::THROW})) return throwStatement();
    if (match({TokenType::SWITCH})) return switchStatement();
    if (match({TokenType::RETURN})) return returnStatement();
    if (match({TokenType::PRINT})) return printStatement();
    if (match({TokenType::LBRACE})) return block();
    if (match({TokenType::BREAK})) {
        int line = previous().line; match({TokenType::SEMICOLON});
        auto s = std::make_shared<Stmt>(); s->type = StmtType::BREAK; s->line = line;
        return s;
    }
    if (match({TokenType::CONTINUE})) {
        int line = previous().line; match({TokenType::SEMICOLON});
        auto s = std::make_shared<Stmt>(); s->type = StmtType::CONTINUE; s->line = line;
        return s;
    }
    return exprStatement();
}

StmtPtr Parser::ifStatement() {
    int line = previous().line;
    bool paren = match({TokenType::LPAREN});
    ExprPtr cond = expression();
    if (paren) consume(TokenType::RPAREN, "expected ')' after the condition.");
    consume(TokenType::LBRACE, "expected '{' for the if block.");
    StmtPtr thenB = block();
    StmtPtr elseB = nullptr;
    if (match({TokenType::ELSE})) {
        if (match({TokenType::IF})) {
            elseB = ifStatement();
        } else {
            consume(TokenType::LBRACE, "expected '{' for the else block.");
            elseB = block();
        }
    }
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::IF; s->line = line;
    s->condition = cond; s->thenBranch = thenB; s->elseBranch = elseB;
    return s;
}

StmtPtr Parser::whileStatement() {
    int line = previous().line;
    bool paren = match({TokenType::LPAREN});
    ExprPtr cond = expression();
    if (paren) consume(TokenType::RPAREN, "expected ')' after the condition.");
    consume(TokenType::LBRACE, "expected '{' for the loop body.");
    StmtPtr body = block();
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::WHILE; s->line = line;
    s->condition = cond; s->thenBranch = body;
    return s;
}

StmtPtr Parser::forStatement() {
    int line = previous().line;
    bool paren = match({TokenType::LPAREN});

    // Look ahead for the for-in form: "ton" "." IDENTIFIER "in" ...
    // If the "in" keyword never shows up, rewind and fall through to the classic C-style for.
    if (check(TokenType::TON) && peekNext().type == TokenType::DOT) {
        int savedPos = current;
        advance(); advance(); // consume "ton" "."
        if (check(TokenType::IDENTIFIER)) {
            Token itemName = advance();
            if (match({TokenType::IN})) {
                ExprPtr iterable = expression();
                if (paren) consume(TokenType::RPAREN, "expected ')' after the for-in collection.");
                consume(TokenType::LBRACE, "expected '{' for the for-in body.");
                StmtPtr body = block();

                auto s = std::make_shared<Stmt>();
                s->type = StmtType::FOR_IN; s->line = line;
                s->name = itemName.lexeme;
                s->condition = iterable;  // reused field: the collection being iterated
                s->thenBranch = body;     // reused field: the loop body
                return s;
            }
        }
        current = savedPos;
    }

    StmtPtr init = nullptr;
    if (match({TokenType::SEMICOLON})) {
        init = nullptr;
    } else if (check(TokenType::TON)) {
        init = tonDeclaration();
    } else {
        init = exprStatement();
    }

    ExprPtr cond = nullptr;
    if (!check(TokenType::SEMICOLON)) cond = expression();
    consume(TokenType::SEMICOLON, "expected ';' after the for condition.");

    ExprPtr incr = nullptr;
    if (paren) {
        if (!check(TokenType::RPAREN)) incr = expression();
        consume(TokenType::RPAREN, "expected ')' after the for clause.");
    } else {
        if (!check(TokenType::LBRACE)) incr = expression();
    }

    consume(TokenType::LBRACE, "expected '{' for the for body.");
    StmtPtr body = block();

    auto s = std::make_shared<Stmt>();
    s->type = StmtType::FOR; s->line = line;
    s->forInit = init; s->forCondition = cond; s->forIncrement = incr; s->forBody = body;
    return s;
}

// "try { ... } catch (ton.err) { ... }" — runs the try block, and if it raises any
// runtime error (native or via "throw"), binds the error message into "err" (as a
// string) and runs the catch block instead of letting the program crash.
// "try { } catch (e) { }", "try { } finally { }", or "try { } catch (e) { }
// finally { }" — at least one of catch/finally must be present. Without a
// catch clause, an error inside the try block is never swallowed: finally
// still runs, then the error keeps propagating (see Interpreter::execute).
StmtPtr Parser::tryStatement() {
    int line = previous().line;
    consume(TokenType::LBRACE, "expected '{' after 'try'.");
    StmtPtr tryBlock = block();

    std::string errorVarName;
    StmtPtr catchBlock = nullptr;
    if (match({TokenType::CATCH})) {
        consume(TokenType::LPAREN, "expected '(' after 'catch'.");
        if (check(TokenType::TON)) {
            advance();
            consume(TokenType::DOT, "expected '.' after 'ton'.");
            errorVarName = consume(TokenType::IDENTIFIER, "expected an error variable name.").lexeme;
        } else {
            errorVarName = consume(TokenType::IDENTIFIER, "expected an error variable name.").lexeme;
        }
        consume(TokenType::RPAREN, "expected ')' after the catch variable.");
        consume(TokenType::LBRACE, "expected '{' for the catch block.");
        catchBlock = block();
    }

    StmtPtr finallyBlock = nullptr;
    if (match({TokenType::FINALLY})) {
        consume(TokenType::LBRACE, "expected '{' after 'finally'.");
        finallyBlock = block();
    }

    if (!catchBlock && !finallyBlock) {
        error(previous(), "expected 'catch' or 'finally' after the try block.");
    }

    auto s = std::make_shared<Stmt>();
    s->type = StmtType::TRY_CATCH; s->line = line;
    s->name = errorVarName;
    s->thenBranch = tryBlock;   // reused field: the try block
    s->elseBranch = catchBlock; // reused field: the catch block (null if none)
    s->finallyBranch = finallyBlock;
    return s;
}

// "throw <expression>" — raises a runtime error carrying the given value's string form.
// Catchable by an enclosing try/catch; otherwise it stops the program like any other error.
StmtPtr Parser::throwStatement() {
    int line = previous().line;
    ExprPtr value = expression();
    match({TokenType::SEMICOLON});
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::THROW; s->line = line; s->expr = value;
    return s;
}

// "switch (subject) { case v1, v2: { ... } case v3: { ... } default: { ... } }"
// — no fallthrough: each matching case (or "default" if none match) runs its
// own block and the switch is done. Parentheses around the subject are
// optional, like if/while/for.
StmtPtr Parser::switchStatement() {
    int line = previous().line;
    bool paren = match({TokenType::LPAREN});
    ExprPtr subject = expression();
    if (paren) consume(TokenType::RPAREN, "expected ')' after the switch value.");
    consume(TokenType::LBRACE, "expected '{' to start the switch body.");

    std::vector<std::pair<std::vector<ExprPtr>, StmtPtr>> cases;
    StmtPtr defaultBlock = nullptr;

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (match({TokenType::CASE})) {
            std::vector<ExprPtr> values;
            values.push_back(expression());
            while (match({TokenType::COMMA})) values.push_back(expression());
            consume(TokenType::COLON, "expected ':' after the case value(s).");
            consume(TokenType::LBRACE, "expected '{' for the case body.");
            cases.push_back({values, block()});
        } else if (match({TokenType::DEFAULT})) {
            consume(TokenType::COLON, "expected ':' after 'default'.");
            consume(TokenType::LBRACE, "expected '{' for the default body.");
            defaultBlock = block();
        } else {
            error(peek(), "expected 'case' or 'default' inside a switch body.");
        }
    }
    consume(TokenType::RBRACE, "expected '}' at the end of the switch.");

    auto s = std::make_shared<Stmt>();
    s->type = StmtType::SWITCH; s->line = line;
    s->condition = subject;      // reused field: the switch subject
    s->switchCases = cases;
    s->elseBranch = defaultBlock; // reused field: the default block
    return s;
}

StmtPtr Parser::returnStatement() {
    int line = previous().line;
    ExprPtr value = nullptr;
    if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE)) value = expression();
    match({TokenType::SEMICOLON});
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::RETURN; s->line = line; s->expr = value;
    return s;
}

StmtPtr Parser::printStatement() {
    int line = previous().line;
    consume(TokenType::LPAREN, "expected '(' after 'print'.");
    ExprPtr value = expression();
    consume(TokenType::RPAREN, "expected ')' after the print argument.");
    match({TokenType::SEMICOLON});
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::PRINT; s->line = line; s->expr = value;
    return s;
}

StmtPtr Parser::block() {
    int line = previous().line;
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBRACE) && !isAtEnd()) stmts.push_back(declaration());
    consume(TokenType::RBRACE, "expected '}' at the end of the block.");
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::BLOCK; s->line = line; s->statements = stmts;
    return s;
}

StmtPtr Parser::exprStatement() {
    int line = peek().line;
    ExprPtr e = expression();
    match({TokenType::SEMICOLON});
    auto s = std::make_shared<Stmt>();
    s->type = StmtType::EXPR_STMT; s->line = line; s->expr = e;
    return s;
}

// ---- Expressions ----

ExprPtr Parser::expression() { return assignment(); }

// Builds the ASSIGN (for a plain variable) or INDEX-with-value (for arr[i] / dict[k])
// node that both "=" and the desugared compound-assignment operators produce.
ExprPtr Parser::makeAssignTarget(const ExprPtr& target, const ExprPtr& value, const Token& opToken) {
    if (target->type == ExprType::VARIABLE) {
        auto a = std::make_shared<Expr>();
        a->type = ExprType::ASSIGN; a->line = opToken.line;
        a->name = target->name; a->value = value;
        return a;
    }
    if (target->type == ExprType::INDEX) {
        auto a = std::make_shared<Expr>();
        a->type = ExprType::INDEX; a->line = opToken.line;
        a->indexTarget = target->indexTarget; a->indexValue = target->indexValue;
        a->value = value;
        return a;
    }
    error(opToken, "invalid assignment target.");
}

ExprPtr Parser::assignment() {
    ExprPtr expr = ternary();

    if (match({TokenType::EQUAL})) {
        Token eq = previous();
        return makeAssignTarget(expr, assignment(), eq);
    }

    // Compound assignment: "ton.x += 1" desugars to "ton.x = ton.x + 1" (and similarly
    // for -=, *=, /=, %=), so the interpreter never needs to know these operators exist.
    static const std::vector<std::pair<TokenType, TokenType>> compoundOps = {
        {TokenType::PLUS_EQUAL, TokenType::PLUS},
        {TokenType::MINUS_EQUAL, TokenType::MINUS},
        {TokenType::STAR_EQUAL, TokenType::STAR},
        {TokenType::SLASH_EQUAL, TokenType::SLASH},
        {TokenType::PERCENT_EQUAL, TokenType::PERCENT},
        {TokenType::AMPERSAND_EQUAL, TokenType::AMPERSAND},
        {TokenType::PIPE_EQUAL, TokenType::PIPE},
        {TokenType::CARET_EQUAL, TokenType::CARET},
        {TokenType::LESS_LESS_EQUAL, TokenType::LESS_LESS},
        {TokenType::GREATER_GREATER_EQUAL, TokenType::GREATER_GREATER},
    };
    for (auto& [tokenType, binaryOp] : compoundOps) {
        if (check(tokenType)) {
            Token opTok = advance();
            ExprPtr rhs = assignment();
            auto binary = std::make_shared<Expr>();
            binary->type = ExprType::BINARY; binary->line = opTok.line;
            binary->left = expr; binary->op = binaryOp; binary->right = rhs;
            return makeAssignTarget(expr, binary, opTok);
        }
    }

    return expr;
}

// "cond ? whenTrue : whenFalse" — the condition is "condition", the two branches
// reuse "left"/"right" (never used together with TERNARY's condition field elsewhere).
ExprPtr Parser::ternary() {
    ExprPtr expr = nilCoalesce();
    if (match({TokenType::QUESTION})) {
        Token q = previous();
        ExprPtr whenTrue = expression();
        consume(TokenType::COLON, "expected ':' in the ternary expression.");
        ExprPtr whenFalse = ternary();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::TERNARY; e->line = q.line;
        e->condition = expr; e->left = whenTrue; e->right = whenFalse;
        return e;
    }
    return expr;
}

// "a ?? b" — evaluates to "a" unless it's nil, in which case "b" runs instead.
// Built as a LOGICAL node (not BINARY) so the interpreter short-circuits:
// "b" is never evaluated at all when "a" isn't nil.
ExprPtr Parser::nilCoalesce() {
    ExprPtr expr = logicOr();
    while (match({TokenType::QUESTION_QUESTION})) {
        Token op = previous(); ExprPtr right = logicOr();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::LOGICAL; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

ExprPtr Parser::logicOr() {
    ExprPtr expr = logicAnd();
    while (match({TokenType::OR})) {
        Token op = previous(); ExprPtr right = logicAnd();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::LOGICAL; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

ExprPtr Parser::logicAnd() {
    ExprPtr expr = bitwise();
    while (match({TokenType::AND})) {
        Token op = previous(); ExprPtr right = bitwise();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::LOGICAL; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

// "a & b", "a | b", "a ^ b" — all one precedence level (left to right), rather
// than the three separate levels C uses; a small language doesn't need
// `a | b & c` to be meaningfully different in precedence from `a & b | c`,
// and parentheses read better than that ordering anyway.
ExprPtr Parser::bitwise() {
    ExprPtr expr = equality();
    while (match({TokenType::AMPERSAND, TokenType::PIPE, TokenType::CARET})) {
        Token op = previous(); ExprPtr right = equality();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::BINARY; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();
    while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
        Token op = previous(); ExprPtr right = comparison();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::BINARY; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

// Also handles "value in collection" (membership test on an array, a dict's
// keys, or a substring of a string) at the same precedence as < <= > >=.
ExprPtr Parser::comparison() {
    ExprPtr expr = shift();
    while (match({TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::IN})) {
        Token op = previous(); ExprPtr right = shift();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::BINARY; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

ExprPtr Parser::shift() {
    ExprPtr expr = term();
    while (match({TokenType::LESS_LESS, TokenType::GREATER_GREATER})) {
        Token op = previous(); ExprPtr right = term();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::BINARY; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous(); ExprPtr right = factor();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::BINARY; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = unary();
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous(); ExprPtr right = unary();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::BINARY; e->line = op.line;
        e->left = expr; e->op = op.type; e->right = right;
        expr = e;
    }
    return expr;
}

ExprPtr Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS, TokenType::TILDE})) {
        Token op = previous(); ExprPtr right = unary();
        auto e = std::make_shared<Expr>();
        e->type = ExprType::UNARY; e->line = op.line;
        e->op = op.type; e->operand = right;
        return e;
    }
    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();
    while (true) {
        if (match({TokenType::LPAREN})) {
            expr = finishCall(expr);
        } else if (match({TokenType::LBRACKET})) {
            Token br = previous();
            ExprPtr idx = expression();
            consume(TokenType::RBRACKET, "expected ']' after the index.");
            auto e = std::make_shared<Expr>();
            e->type = ExprType::INDEX; e->line = br.line;
            e->indexTarget = expr; e->indexValue = idx;
            expr = e;
        } else if (check(TokenType::PLUS_PLUS) || check(TokenType::MINUS_MINUS)) {
            // Postfix "ton.x++" / "ton.x--" desugars to "ton.x = ton.x + 1" / "ton.x = ton.x - 1".
            Token opTok = advance();
            TokenType binaryOp = (opTok.type == TokenType::PLUS_PLUS) ? TokenType::PLUS : TokenType::MINUS;
            auto one = std::make_shared<Expr>();
            one->type = ExprType::LITERAL; one->line = opTok.line; one->isNumber = true; one->litNumber = 1;
            auto binary = std::make_shared<Expr>();
            binary->type = ExprType::BINARY; binary->line = opTok.line;
            binary->left = expr; binary->op = binaryOp; binary->right = one;
            expr = makeAssignTarget(expr, binary, opTok);
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::finishCall(ExprPtr callee) {
    Token paren = previous();
    std::vector<ExprPtr> args;
    if (!check(TokenType::RPAREN)) {
        do { args.push_back(expression()); } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, "expected ')' after the arguments.");
    auto e = std::make_shared<Expr>();
    e->type = ExprType::CALL; e->line = paren.line;
    e->callee = callee; e->args = args;
    return e;
}

// Parse "ton.identifiant" comme une expression (variable ou callee d'appel), or
// "ton.function(...) { ... }" as an inline function value (a callback expression).
ExprPtr Parser::tonReference() {
    Token tonTok = advance(); // TON
    consume(TokenType::DOT, "expected '.' after 'ton'.");
    if (check(TokenType::FUNCTION)) {
        advance();
        return functionExpression(tonTok.line);
    }
    Token name = consume(TokenType::IDENTIFIER, "expected a name after 'ton.'.");
    auto e = std::make_shared<Expr>();
    e->type = ExprType::VARIABLE;
    e->line = tonTok.line;
    e->name = name.lexeme;
    return e;
}

// "ton.function [name](params) { body }" as an expression — the name is optional
// and only used for a friendlier debug label (e.g. in a stack trace); it's never
// bound as a variable by itself, unlike a top-level "ton.function name(...) {}" declaration.
ExprPtr Parser::functionExpression(int line) {
    std::string fnName;
    if (check(TokenType::IDENTIFIER)) fnName = advance().lexeme;

    consume(TokenType::LPAREN, "expected '(' after 'function'.");
    ParamList params = parseParamList();
    consume(TokenType::RPAREN, "expected ')' after the parameters.");
    consume(TokenType::LBRACE, "expected '{' before the function body.");
    StmtPtr body = block();

    auto e = std::make_shared<Expr>();
    e->type = ExprType::FUNCTION_EXPR; e->line = line;
    e->name = fnName; e->fnParams = params.names; e->fnParamDefaults = params.defaults;
    e->fnHasRestParam = params.hasRest; e->fnBody = body;
    return e;
}

ExprPtr Parser::primary() {
    if (match({TokenType::FALSE})) {
        auto e = std::make_shared<Expr>(); e->type = ExprType::LITERAL; e->line = previous().line;
        e->isBoolLit = true; e->litBool = false; return e;
    }
    if (match({TokenType::TRUE})) {
        auto e = std::make_shared<Expr>(); e->type = ExprType::LITERAL; e->line = previous().line;
        e->isBoolLit = true; e->litBool = true; return e;
    }
    if (match({TokenType::NIL})) {
        auto e = std::make_shared<Expr>(); e->type = ExprType::LITERAL; e->line = previous().line;
        e->isNil = true; return e;
    }
    if (match({TokenType::NUMBER})) {
        auto e = std::make_shared<Expr>(); e->type = ExprType::LITERAL; e->line = previous().line;
        e->isNumber = true; e->litNumber = previous().numberValue; return e;
    }
    if (match({TokenType::STRING})) {
        auto e = std::make_shared<Expr>(); e->type = ExprType::LITERAL; e->line = previous().line;
        e->isString = true; e->litString = previous().stringValue; return e;
    }
    if (check(TokenType::TON)) {
        return tonReference();
    }
    if (match({TokenType::IDENTIFIER})) {
        auto e = std::make_shared<Expr>(); e->type = ExprType::VARIABLE; e->line = previous().line;
        e->name = previous().lexeme; return e;
    }
    if (match({TokenType::LPAREN})) {
        ExprPtr inner = expression();
        consume(TokenType::RPAREN, "expected ')' after the expression.");
        auto e = std::make_shared<Expr>(); e->type = ExprType::GROUPING; e->line = previous().line;
        e->inner = inner; return e;
    }
    if (match({TokenType::LBRACKET})) {
        Token br = previous();
        std::vector<ExprPtr> elems;
        if (!check(TokenType::RBRACKET)) {
            do {
                if (match({TokenType::ELLIPSIS})) {
                    Token dots = previous();
                    auto spread = std::make_shared<Expr>();
                    spread->type = ExprType::SPREAD; spread->line = dots.line;
                    spread->operand = expression();
                    elems.push_back(spread);
                } else {
                    elems.push_back(expression());
                }
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RBRACKET, "expected ']' after the array elements.");
        auto e = std::make_shared<Expr>(); e->type = ExprType::ARRAY; e->line = br.line;
        e->elements = elems; return e;
    }
    // Dict literal: { key: value, key2: value2, ... }. A key is either a bare
    // identifier (treated as its own name, e.g. `name: "kuro"`) or a string literal
    // (needed for keys that aren't valid identifiers, e.g. `"first name": "kuro"`).
    if (match({TokenType::LBRACE})) {
        Token br = previous();
        std::vector<std::pair<ExprPtr, ExprPtr>> entries;
        if (!check(TokenType::RBRACE)) {
            do {
                if (match({TokenType::ELLIPSIS})) {
                    Token dots = previous();
                    auto spread = std::make_shared<Expr>();
                    spread->type = ExprType::SPREAD; spread->line = dots.line;
                    spread->operand = expression();
                    entries.push_back({nullptr, spread});
                    continue;
                }
                ExprPtr key;
                if (check(TokenType::STRING)) {
                    Token k = advance();
                    key = std::make_shared<Expr>();
                    key->type = ExprType::LITERAL; key->line = k.line;
                    key->isString = true; key->litString = k.stringValue;
                } else {
                    Token k = consume(TokenType::IDENTIFIER, "expected a key name in the dict literal.");
                    key = std::make_shared<Expr>();
                    key->type = ExprType::LITERAL; key->line = k.line;
                    key->isString = true; key->litString = k.lexeme;
                }
                consume(TokenType::COLON, "expected ':' after the dict key.");
                ExprPtr value = expression();
                entries.push_back({key, value});
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RBRACE, "expected '}' after the dict literal.");
        auto e = std::make_shared<Expr>(); e->type = ExprType::DICT; e->line = br.line;
        e->dictEntries = entries; return e;
    }
    error(peek(), "expected an expression.");
}
