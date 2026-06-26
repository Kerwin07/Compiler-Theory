#include "codegen.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <vector>

// ============================================================
// JSON AST 解析（与 semantic 共用相同格式）
// ============================================================

static std::string unescapeString(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[++i];
            switch (c) {
                case '\\': r.push_back('\\'); break;
                case '"':  r.push_back('"');  break;
                case 'n':  r.push_back('\n'); break;
                case 'r':  r.push_back('\r'); break;
                case 't':  r.push_back('\t'); break;
                default:   r.push_back('\\'); r.push_back(c); break;
            }
        } else {
            r.push_back(s[i]);
        }
    }
    return r;
}

static void skipWS(const std::string& js, size_t& i) {
    while (i < js.size() && (js[i] == ' ' || js[i] == '\t' || js[i] == '\n' || js[i] == '\r'))
        i++;
}

static bool expectChar(const std::string& js, size_t& i, char expected) {
    skipWS(js, i);
    if (i < js.size() && js[i] == expected) { i++; return true; }
    return false;
}

static bool parseString(const std::string& js, size_t& i, std::string& out) {
    skipWS(js, i);
    if (i >= js.size() || js[i] != '"') return false;
    i++;
    out.clear();
    while (i < js.size() && js[i] != '"') {
        if (js[i] == '\\' && i + 1 < js.size()) { out.push_back(js[i++]); out.push_back(js[i++]); }
        else { out.push_back(js[i++]); }
    }
    if (i >= js.size()) return false;
    i++;
    return true;
}

static ASTNode* parseNode(const std::string& js, size_t& i) {
    skipWS(js, i);
    if (i >= js.size()) return nullptr;
    if (js[i] == 'n') { i += 4; return nullptr; }
    if (js[i] != '{') return nullptr;
    i++;

    ASTNode* node = new ASTNode();
    skipWS(js, i);
    while (i < js.size() && js[i] != '}') {
        std::string key;
        if (!parseString(js, i, key)) { delete node; return nullptr; }
        key = unescapeString(key);
        if (!expectChar(js, i, ':')) { delete node; return nullptr; }
        skipWS(js, i);

        if (key == "sym") {
            std::string val;
            if (!parseString(js, i, val)) { delete node; return nullptr; }
            node->sym = unescapeString(val);
        } else if (key == "lexeme") {
            std::string val;
            if (!parseString(js, i, val)) { delete node; return nullptr; }
            node->lexeme = unescapeString(val);
            node->isToken = true;
        } else if (key == "children") {
            if (!expectChar(js, i, '[')) { delete node; return nullptr; }
            skipWS(js, i);
            while (i < js.size() && js[i] != ']') {
                ASTNode* child = parseNode(js, i);
                if (child) node->children.push_back(child);
                skipWS(js, i);
                if (i < js.size() && js[i] == ',') i++;
            }
            if (!expectChar(js, i, ']')) { delete node; return nullptr; }
        }
        skipWS(js, i);
        if (i < js.size() && js[i] == ',') i++;
    }
    if (!expectChar(js, i, '}')) { delete node; return nullptr; }
    return node;
}

ASTNode* loadAST(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open()) return nullptr;
    std::ostringstream ss;
    ss << fin.rdbuf();
    std::string js = ss.str();
    size_t i = 0;
    return parseNode(js, i);
}

// ============================================================
// 环境
// ============================================================

int Env::get(const std::string& name) const {
    auto it = vars.find(name);
    if (it == vars.end())
        throw std::runtime_error("undefined variable '" + name + "'");
    return it->second;
}

void Env::set(const std::string& name, int val) {
    vars[name] = val;
}

// ============================================================
// ============================================================
// 内置函数 printf / scanf
// ============================================================

static int evalExpr(ASTNode* node, Env& env);

static std::vector<int> evalArgs(ASTNode* argsNode, Env& env) {
    std::vector<int> vals;
    if (!argsNode || argsNode->children.empty()) return vals;
    ASTNode* expr = argsNode->children[0]; // Args -> Expr ArgTail
    vals.push_back(evalExpr(expr, env));
    ASTNode* tail = (argsNode->children.size() >= 2) ? argsNode->children[1] : nullptr;
    while (tail && !tail->children.empty() && tail->sym == "ArgTail") {
        expr = tail->children[1]; // ArgTail -> COMMA Expr ArgTail
        vals.push_back(evalExpr(expr, env));
        tail = (tail->children.size() >= 3) ? tail->children[2] : nullptr;
    }
    return vals;
}

static int builtinCall(const std::string& name, ASTNode* argsNode, Env& env) {
    if (name == "printf") {
        bool first = true;
        ASTNode* cur = argsNode;
        while (cur && !cur->children.empty() && (cur->sym == "Args" || cur->sym == "ArgTail")) {
            ASTNode* expr;
            if (cur->sym == "Args") {
                expr = cur->children[0]; // Args -> Expr ArgTail
            } else {
                expr = cur->children[1]; // ArgTail -> COMMA Expr ArgTail
            }
            if (!first) std::cout << " ";
            first = false;
            // 检查是否为字符串字面量：遍历到 Factor → STRING
            {
                ASTNode* f = expr;
                // Expr → Comp Expr'
                if (f->sym == "Expr" && f->children.size() >= 1) f = f->children[0];
                // Comp → Term Comp'
                if (f->sym == "Comp" && f->children.size() >= 1) f = f->children[0];
                // Term → Power Term'
                if (f->sym == "Term" && f->children.size() >= 1) f = f->children[0];
                // Power → Factor PowerRest
                if (f->sym == "Power" && f->children.size() >= 1) f = f->children[0];
                // Factor → STRING
                if (f->sym == "Factor" && f->children.size() >= 1 &&
                    f->children[0]->isToken && f->children[0]->sym == "STRING") {
                    std::string s = f->children[0]->lexeme;
                    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                        std::cout << s.substr(1, s.size() - 2);
                    else
                        std::cout << s;
                    goto next_arg;
                }
            }
            std::cout << evalExpr(expr, env);
            next_arg:;
            if (cur->sym == "Args" && cur->children.size() >= 2)
                cur = cur->children[1]; // ArgTail
            else if (cur->sym == "ArgTail" && cur->children.size() >= 3)
                cur = cur->children[2]; // next ArgTail
            else
                cur = nullptr;
        }
        std::cout << std::endl;
        return 0;
    }
    if (name == "scanf") {
        int val = 0;
        std::cin >> val;
        return val;
    }
    std::cerr << "undefined function: " << name << std::endl;
    return 0;
}

static int evalFactor(ASTNode* node, Env& env) {
    if (!node) throw std::runtime_error("null Factor");
    if (node->sym == "Factor") {
        if (node->children.empty()) throw std::runtime_error("empty Factor");
        if (node->children[0]->isToken && node->children[0]->sym == "MINUS") {
            return -evalFactor(node->children[1], env);
        }
        // Factor -> ID FactorRest
        if (node->children.size() >= 2 && node->children[1]->sym == "FactorRest") {
            ASTNode* rest = node->children[1];
            if (!rest->children.empty() && rest->children[0]->sym == "LPAREN") {
                std::string funcName = node->children[0]->lexeme;
                ASTNode* argsNode = rest->children[1]; // Args
                return builtinCall(funcName, argsNode, env);
            }
        }
        return evalFactor(node->children[0], env);
    }
    if (node->isToken) {
        if (node->sym == "ID") return env.get(node->lexeme);
        if (node->sym == "INT") return std::stoi(node->lexeme);
        if (node->sym == "STRING") return 0;
        throw std::runtime_error("unexpected token in Factor: " + node->sym);
    }
    if (node->sym == "LPAREN") {
        if (node->children.empty()) throw std::runtime_error("empty paren");
        return evalExpr(node->children[0], env);
    }
    if (node->sym == "MINUS") {
        return -evalFactor(node->children[0], env);
    }
    throw std::runtime_error("unexpected node in Factor: " + node->sym);
}

static int evalPower(ASTNode* node, Env& env) {
    if (!node) throw std::runtime_error("null Power");
    if (node->sym == "Power") {
        int val = evalFactor(node->children[0], env);
        if (node->children.size() >= 2) {
            ASTNode* rest = node->children[1];
            if (!rest->children.empty() && rest->children[0]->sym == "POW") {
                int expVal = evalPower(rest->children[1], env);
                int result = 1;
                for (int i = 0; i < expVal; i++) result *= val;
                return result;
            }
        }
        return val;
    }
    return evalFactor(node, env);
}

static int evalTerm(ASTNode* node, Env& env) {
    int val = evalPower(node->children[0], env);
    ASTNode* tprime = (node->children.size() >= 2) ? node->children[1] : nullptr;
    while (tprime && !tprime->children.empty() && tprime->sym == "Term'") {
        if (tprime->children[0]->sym == "MUL") {
            val *= evalPower(tprime->children[1], env);
        } else if (tprime->children[0]->sym == "DIV") {
            int rhs = evalPower(tprime->children[1], env);
            if (rhs == 0) throw std::runtime_error("division by zero");
            val /= rhs;
        } else {
            break;
        }
        tprime = (tprime->children.size() >= 3) ? tprime->children[2] : nullptr;
    }
    return val;
}

static int evalComp(ASTNode* node, Env& env) {
    // Comp -> Term Comp'
    int val = evalTerm(node->children[0], env);
    ASTNode* cprime = (node->children.size() >= 2) ? node->children[1] : nullptr;
    while (cprime && !cprime->children.empty() && cprime->sym == "Comp'") {
        if (cprime->children[0]->sym == "PLUS") {
            val += evalTerm(cprime->children[1], env);
        } else if (cprime->children[0]->sym == "MINUS") {
            val -= evalTerm(cprime->children[1], env);
        } else {
            break;
        }
        cprime = (cprime->children.size() >= 3) ? cprime->children[2] : nullptr;
    }
    return val;
}

static int evalExpr(ASTNode* node, Env& env) {
    int val = evalComp(node->children[0], env);
    ASTNode* eprime = (node->children.size() >= 2) ? node->children[1] : nullptr;
    while (eprime && !eprime->children.empty() && eprime->sym == "Expr'") {
        const std::string& op = eprime->children[0]->sym;
        int rhs = evalComp(eprime->children[1], env);
        if (op == "EQ")       val = (val == rhs) ? 1 : 0;
        else if (op == "NE")  val = (val != rhs) ? 1 : 0;
        else if (op == "LT")  val = (val < rhs)  ? 1 : 0;
        else if (op == "GT")  val = (val > rhs)  ? 1 : 0;
        else if (op == "LE")  val = (val <= rhs) ? 1 : 0;
        else if (op == "GE")  val = (val >= rhs) ? 1 : 0;
        else break;
        eprime = (eprime->children.size() >= 3) ? eprime->children[2] : nullptr;
    }
    return val;
}

// 计算 StmtTail 中的表达式尾部 (Term' Comp' Expr') 从 ID 开始的值
static int evalStmtTailExpr(ASTNode* tail, const std::string& idName, Env& env) {
    int val = env.get(idName);

    // tail is StmtTail -> Term' Comp' Expr'
    ASTNode* tprime = nullptr;
    ASTNode* cprime = nullptr;
    ASTNode* eprime = nullptr;

    for (auto* c : tail->children) {
        if (c->sym == "Term'") tprime = c;
        if (c->sym == "Comp'") cprime = c;
        if (c->sym == "Expr'") eprime = c;
    }

    while (tprime && !tprime->children.empty()) {
        if (tprime->children[0]->sym == "MUL") {
            val *= evalPower(tprime->children[1], env);
        } else if (tprime->children[0]->sym == "DIV") {
            int rhs = evalPower(tprime->children[1], env);
            if (rhs == 0) throw std::runtime_error("division by zero");
            val /= rhs;
        } else break;
        tprime = (tprime->children.size() >= 3) ? tprime->children[2] : nullptr;
    }

    while (cprime && !cprime->children.empty()) {
        if (cprime->children[0]->sym == "PLUS") {
            val += evalTerm(cprime->children[1], env);
        } else if (cprime->children[0]->sym == "MINUS") {
            val -= evalTerm(cprime->children[1], env);
        } else break;
        cprime = (cprime->children.size() >= 3) ? cprime->children[2] : nullptr;
    }

    while (eprime && !eprime->children.empty()) {
        const std::string& op = eprime->children[0]->sym;
        int rhs = evalComp(eprime->children[1], env);
        if (op == "EQ")       val = (val == rhs) ? 1 : 0;
        else if (op == "NE")  val = (val != rhs) ? 1 : 0;
        else if (op == "LT")  val = (val < rhs)  ? 1 : 0;
        else if (op == "GT")  val = (val > rhs)  ? 1 : 0;
        else if (op == "LE")  val = (val <= rhs) ? 1 : 0;
        else if (op == "GE")  val = (val >= rhs) ? 1 : 0;
        else break;
        eprime = (eprime->children.size() >= 3) ? eprime->children[2] : nullptr;
    }

    return val;
}

// ============================================================
// 语句执行
// ============================================================

static ExecResult execStmtList(ASTNode* node, Env& env, bool& returned, int& retVal, std::ostream& trace);

static ExecResult execStmt(ASTNode* node, Env& env, std::ostream& trace) {
    ExecResult r;
    if (!node) return r;

    const std::string& first = node->children.empty() ? "" : node->children[0]->sym;

    if (first == "SEMI") {
        return r;
    }

    if (first == "RETURN") {
        trace << "  RETURN ";
        int val = evalExpr(node->children[1], env);
        trace << val << "\n";
        r.hasReturn = true;
        r.returnValue = val;
        return r;
    }

    if (first == "ID") {
        ASTNode* tail = node->children[1];
        if (tail->sym == "StmtTail") {
            if (!tail->children.empty() && tail->children[0]->sym == "LPAREN") {
                // 函数调用语句: ID ( Args ) ;
                std::string funcName = node->children[0]->lexeme;
                ASTNode* argsNode = tail->children[1]; // Args
                trace << "  call " << funcName << "\n";
                builtinCall(funcName, argsNode, env);
            } else if (!tail->children.empty() && tail->children[0]->sym == "ASSIGN") {
                // 赋值: ID = Expr
                std::string var = node->children[0]->lexeme;
                int val = evalExpr(tail->children[1], env);
                env.set(var, val);
                trace << "  " << var << " = " << val << "\n";
            } else {
                // 表达式语句: ID + / * == ...
                int val = evalStmtTailExpr(tail, node->children[0]->lexeme, env);
                trace << "  expr(" << node->children[0]->lexeme << "...) = " << val << "\n";
            }
        }
        return r;
    }

    if (first == "IF") {
        int cond = evalExpr(node->children[2], env);
        trace << "  IF (cond=" << cond << ")\n";
        if (cond) {
            r = execStmt(node->children[4], env, trace);
            return r;
        }
        // else
        if (node->children.size() >= 6) {
            ASTNode* elseOpt = node->children[5];
            if (elseOpt->sym == "ElseOpt" && !elseOpt->children.empty()) {
                trace << "  ELSE\n";
                r = execStmt(elseOpt->children[1], env, trace);
                return r;
            }
        }
        return r;
    }

    if (first == "WHILE") {
        while (true) {
            int cond = evalExpr(node->children[2], env);
            trace << "  WHILE (cond=" << cond << ")\n";
            if (!cond) break;
            r = execStmt(node->children[4], env, trace);
            if (r.hasReturn) return r;
        }
        return r;
    }

    if (first == "INT_KW") {
        // typed declaration: int x; or int x = Expr;
        std::string var = node->children[1]->lexeme;
        ASTNode* declRest = node->children[2];
        int val = 0;
        if (!declRest->children.empty() && declRest->children[0]->sym == "ASSIGN") {
            val = evalExpr(declRest->children[1], env);
        }
        env.set(var, val);
        trace << "  int " << var << " = " << val << "\n";
        return r;
    }

    if (first == "LBRACE") {
        // LBRACE StmtList RBRACE -> StmtList is child[1]
        bool ret = false;
        int rv = 0;
        execStmtList(node->children[1], env, ret, rv, trace);
        if (ret) { r.hasReturn = true; r.returnValue = rv; }
        return r;
    }

    return r;
}

static ExecResult execStmtList(ASTNode* node, Env& env, bool& returned, int& retVal, std::ostream& trace) {
    ExecResult r;
    // StmtList -> Stmt StmtList | eps
    while (node && node->sym == "StmtList" && !node->children.empty()) {
        ASTNode* stmt = node->children[0];
        r = execStmt(stmt, env, trace);
        if (r.hasReturn) { returned = true; retVal = r.returnValue; return r; }
        node = (node->children.size() >= 2) ? node->children[1] : nullptr;
    }
    return r;
}

// ============================================================
// 顶层
// ============================================================

int executeProgram(ASTNode* root) {
    if (!root) return 0;

    Env env;

    std::ofstream trace("execution_trace.txt");
    std::ostream& out = trace.is_open() ? trace : std::cout;

    out << "=== Execution Trace ===\n";

    if (root->children.empty()) return 0;

    const std::string& first = root->children[0]->sym;

    if (first == "INT_KW") {
        // FuncDef: INT_KW ID LPAREN RPAREN Block
        // Block is children[4]; Block -> LBRACE StmtList RBRACE
        ASTNode* block = root->children[4];
        if (block && block->children.size() >= 2) {
            ASTNode* stmtList = block->children[1];
            bool ret = false;
            int rv = 0;
            execStmtList(stmtList, env, ret, rv, out);
            if (ret) {
                out << "Program returned: " << rv << "\n";
                std::cout << "Program returned: " << rv << std::endl;
                return rv;
            }
        }
        out << "Program returned: 0\n";
        std::cout << "Program returned: 0" << std::endl;
        return 0;
    }

    // root is S -> StmtList
    ASTNode* stmtList = root->children[0];
    bool ret = false;
    int rv = 0;
    execStmtList(stmtList, env, ret, rv, out);
    if (ret) {
        out << "Program returned: " << rv << "\n";
        std::cout << "Program returned: " << rv << std::endl;
        return rv;
    }

    out << "Program finished (no return)\n";
    std::cout << "Program finished (no return)" << std::endl;
    return 0;
}
