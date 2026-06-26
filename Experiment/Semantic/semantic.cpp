#include "semantic.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <deque>
#include <functional>
#include <stdexcept>
#include <cmath>

static std::string escapeString(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        switch (c) {
            case '\\': r += "\\\\"; break;
            case '"':  r += "\\\""; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:   r.push_back(c); break;
        }
    }
    return r;
}

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

static void skipWhitespace(const std::string& js, size_t& i) {
    while (i < js.size() && (js[i] == ' ' || js[i] == '\t' || js[i] == '\n' || js[i] == '\r'))
        i++;
}

static bool expectChar(const std::string& js, size_t& i, char expected) {
    skipWhitespace(js, i);
    if (i < js.size() && js[i] == expected) { i++; return true; }
    return false;
}

static bool parseString(const std::string& js, size_t& i, std::string& out) {
    skipWhitespace(js, i);
    if (i >= js.size() || js[i] != '"') return false;
    i++;
    out.clear();
    while (i < js.size() && js[i] != '"') {
        if (js[i] == '\\' && i + 1 < js.size()) {
            out.push_back(js[i]);
            i++;
            out.push_back(js[i]);
            i++;
        } else {
            out.push_back(js[i]);
            i++;
        }
    }
    if (i >= js.size()) return false;
    i++;
    return true;
}

static bool parseLiteral(const std::string& js, size_t& i, std::string& out) {
    skipWhitespace(js, i);
    size_t start = i;
    while (i < js.size() && js[i] != ',' && js[i] != '}' && js[i] != ']' && js[i] != ':' && js[i] != '"')
        i++;
    out = js.substr(start, i - start);
    return !out.empty();
}

static ASTNode* parseNode(const std::string& js, size_t& i) {
    skipWhitespace(js, i);
    if (i >= js.size()) return nullptr;

    if (js[i] == 'n') {
        i += 4;
        return nullptr;
    }

    if (js[i] != '{') return nullptr;
    i++;

    ASTNode* node = new ASTNode();

    skipWhitespace(js, i);
    while (i < js.size() && js[i] != '}') {
        std::string key;
        if (!parseString(js, i, key)) { delete node; return nullptr; }
        key = unescapeString(key);

        if (!expectChar(js, i, ':')) { delete node; return nullptr; }
        skipWhitespace(js, i);

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
            skipWhitespace(js, i);
            while (i < js.size() && js[i] != ']') {
                ASTNode* child = parseNode(js, i);
                if (child) node->children.push_back(child);
                skipWhitespace(js, i);
                if (i < js.size() && js[i] == ',') i++;
            }
            if (!expectChar(js, i, ']')) { delete node; return nullptr; }
        }
        skipWhitespace(js, i);
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
// 表达式求值（含类型推断）— 递归遍历 AST
// ============================================================

static EvalResult evalFactor(ASTNode* node,
                             Scope& scope,
                             std::vector<std::string>& diag);

static EvalResult evalPower(ASTNode* node,
                            Scope& scope,
                            std::vector<std::string>& diag);

static EvalResult evalTerm(ASTNode* node,
                           Scope& scope,
                           std::vector<std::string>& diag);

static EvalResult evalComp(ASTNode* node,
                           Scope& scope,
                           std::vector<std::string>& diag);

static EvalResult evalExprInternal(ASTNode* node,
                                   Scope& scope,
                                   std::vector<std::string>& diag);

static EvalResult promoteToFloat(const EvalResult& v) {
    EvalResult r = v;
    if (v.type == VarType::TY_INT) {
        r.type = VarType::TY_FLOAT;
        r.floatVal = (double)v.intVal;
    }
    return r;
}

static VarType resultType(VarType a, VarType b, const char* op) {
    if (a == VarType::TY_ERROR || b == VarType::TY_ERROR) return VarType::TY_ERROR;
    if (a == VarType::TY_FLOAT || b == VarType::TY_FLOAT) return VarType::TY_FLOAT;
    return VarType::TY_INT;
}

static EvalResult makeInt(int val) {
    EvalResult r;
    r.type = VarType::TY_INT;
    r.intVal = val;
    return r;
}

static EvalResult makeBool(bool val) {
    EvalResult r;
    r.type = VarType::TY_BOOL;
    r.intVal = val ? 1 : 0;
    return r;
}

static EvalResult evalFactor(ASTNode* node,
                             Scope& scope,
                             std::vector<std::string>& diag) {
    if (!node) {
        diag.push_back("null node in Factor");
        return {};
    }

    if (node->sym == "Factor") {
        if (node->children.empty()) {
            diag.push_back("empty Factor");
            return {};
        }
        if (node->children[0]->isToken && node->children[0]->sym == "MINUS") {
            EvalResult inner = evalFactor(node->children[1], scope, diag);
            if (inner.type == VarType::TY_ERROR) return {};
            EvalResult r = inner;
            r.intVal = -inner.intVal;
            r.floatVal = -inner.floatVal;
            return r;
        }
        if (node->children.size() >= 2 && node->children[1]->sym == "FactorRest") {
            ASTNode* rest = node->children[1];
            if (!rest->children.empty() && rest->children[0]->sym == "LPAREN") {
                EvalResult r;
                r.type = VarType::TY_INT;
                return r;
            }
        }
        return evalFactor(node->children[0], scope, diag);
    }

    if (node->isToken) {
        if (node->sym == "ID") {
            const SymbolEntry* entry = scope.lookup(node->lexeme);
            if (!entry || !entry->defined) {
                diag.push_back("undefined variable '" + node->lexeme + "' in expression");
                return {};
            }
            EvalResult r;
            r.type = entry->type;
            r.intVal = entry->value;
            r.floatVal = (double)entry->value;
            return r;
        }
        if (node->sym == "INT") {
            EvalResult r;
            r.type = VarType::TY_INT;
            r.intVal = std::stoi(node->lexeme);
            r.floatVal = (double)r.intVal;
            return r;
        }
        if (node->sym == "FLOAT") {
            EvalResult r;
            r.type = VarType::TY_FLOAT;
            r.floatVal = std::stod(node->lexeme);
            r.intVal = (int)r.floatVal;
            return r;
        }
        diag.push_back("unexpected token in Factor: " + node->sym);
        return {};
    }

    if (node->sym == "LPAREN") {
        if (node->children.empty()) {
            diag.push_back("empty paren");
            return {};
        }
        return evalExprInternal(node->children[0], scope, diag);
    }

    diag.push_back("unexpected node in Factor: " + node->sym);
    return {};
}

static EvalResult evalPower(ASTNode* node,
                            Scope& scope,
                            std::vector<std::string>& diag) {
    if (!node) { diag.push_back("null Power"); return {}; }

    if (node->sym == "Power") {
        EvalResult val = evalFactor(node->children[0], scope, diag);
        if (node->children.size() >= 2) {
            ASTNode* rest = node->children[1];
            if (!rest->children.empty() && rest->children[0]->sym == "POW") {
                EvalResult right = evalPower(rest->children[1], scope, diag);
                if (val.type == VarType::TY_ERROR || right.type == VarType::TY_ERROR) return {};

                double base = (val.type == VarType::TY_FLOAT) ? val.floatVal : (double)val.intVal;
                double exp  = (right.type == VarType::TY_FLOAT) ? right.floatVal : (double)right.intVal;

                EvalResult res;
                res.type = (val.type == VarType::TY_FLOAT || right.type == VarType::TY_FLOAT)
                               ? VarType::TY_FLOAT : VarType::TY_INT;
                double dval = std::pow(base, exp);
                if (res.type == VarType::TY_INT) {
                    res.intVal = (int)dval;
                    res.floatVal = dval;
                } else {
                    res.floatVal = dval;
                    res.intVal = (int)dval;
                }
                return res;
            }
        }
        return val;
    }

    return evalFactor(node, scope, diag);
}

static EvalResult evalTerm(ASTNode* node,
                           Scope& scope,
                           std::vector<std::string>& diag) {
    EvalResult val = evalPower(node->children[0], scope, diag);
    ASTNode* tprime = (node->children.size() >= 2) ? node->children[1] : nullptr;

    while (tprime && !tprime->children.empty() && tprime->sym == "Term'") {
        if (tprime->children[0]->sym == "MUL") {
            EvalResult rhs = evalPower(tprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            VarType restype = resultType(val.type, rhs.type, "*");
            if (restype == VarType::TY_FLOAT) {
                val = promoteToFloat(val);
                rhs = promoteToFloat(rhs);
                val.floatVal *= rhs.floatVal;
                val.intVal = (int)val.floatVal;
            } else {
                val.intVal *= rhs.intVal;
                val.floatVal = (double)val.intVal;
            }
            val.type = restype;
        } else if (tprime->children[0]->sym == "DIV") {
            EvalResult rhs = evalPower(tprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            int rhsInt = rhs.intVal;
            double rhsFloat = rhs.floatVal;
            if (rhsFloat == 0.0) {
                diag.push_back("division by zero");
                return {};
            }
            VarType restype = (val.type == VarType::TY_FLOAT || rhs.type == VarType::TY_FLOAT)
                                  ? VarType::TY_FLOAT : VarType::TY_INT;
            val = promoteToFloat(val);
            val.floatVal /= rhsFloat;
            val.intVal = (int)val.floatVal;
            val.type = restype;
        } else {
            break;
        }
        tprime = (tprime->children.size() >= 3) ? tprime->children[2] : nullptr;
    }
    return val;
}

static EvalResult evalComp(ASTNode* node,
                           Scope& scope,
                           std::vector<std::string>& diag) {
    EvalResult val = evalTerm(node->children[0], scope, diag);
    ASTNode* cprime = (node->children.size() >= 2) ? node->children[1] : nullptr;

    while (cprime && !cprime->children.empty() && cprime->sym == "Comp'") {
        if (cprime->children[0]->sym == "PLUS") {
            EvalResult rhs = evalTerm(cprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            VarType restype = resultType(val.type, rhs.type, "+");
            if (restype == VarType::TY_FLOAT) {
                val = promoteToFloat(val);
                rhs = promoteToFloat(rhs);
                val.floatVal += rhs.floatVal;
                val.intVal = (int)val.floatVal;
            } else {
                val.intVal += rhs.intVal;
                val.floatVal = (double)val.intVal;
            }
            val.type = restype;
        } else if (cprime->children[0]->sym == "MINUS") {
            EvalResult rhs = evalTerm(cprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            VarType restype = resultType(val.type, rhs.type, "-");
            if (restype == VarType::TY_FLOAT) {
                val = promoteToFloat(val);
                rhs = promoteToFloat(rhs);
                val.floatVal -= rhs.floatVal;
                val.intVal = (int)val.floatVal;
            } else {
                val.intVal -= rhs.intVal;
                val.floatVal = (double)val.intVal;
            }
            val.type = restype;
        } else {
            break;
        }
        cprime = (cprime->children.size() >= 3) ? cprime->children[2] : nullptr;
    }
    return val;
}

static EvalResult evalExprInternal(ASTNode* node,
                                   Scope& scope,
                                   std::vector<std::string>& diag) {
    EvalResult val = evalComp(node->children[0], scope, diag);
    ASTNode* eprime = (node->children.size() >= 2) ? node->children[1] : nullptr;

    while (eprime && !eprime->children.empty() && eprime->sym == "Expr'") {
        const std::string& op = eprime->children[0]->sym;
        EvalResult rhs = evalComp(eprime->children[1], scope, diag);
        if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};

        double va = (val.type == VarType::TY_FLOAT) ? val.floatVal : (double)val.intVal;
        double vb = (rhs.type == VarType::TY_FLOAT) ? rhs.floatVal : (double)rhs.intVal;

        bool result = false;
        if (op == "EQ")       result = (va == vb);
        else if (op == "NE")  result = (va != vb);
        else if (op == "LT")  result = (va < vb);
        else if (op == "GT")  result = (va > vb);
        else if (op == "LE")  result = (va <= vb);
        else if (op == "GE")  result = (va >= vb);
        else break;

        val = makeBool(result);
        eprime = (eprime->children.size() >= 3) ? eprime->children[2] : nullptr;
    }
    return val;
}

EvalResult evalExpr(ASTNode* expr,
                    const std::map<std::string, SymbolEntry>& symbols,
                    std::vector<std::string>& diag) {
    Scope tempScope;
    for (auto& kv : symbols) {
        tempScope.symbols[kv.first] = kv.second;
    }
    return evalExprInternal(expr, tempScope, diag);
}

// ============================================================
// 表达式尾部求值（ID StmtTail 中 StmtTail = Term' Comp' Expr'）
// ============================================================

static EvalResult evalStmtTailExpr(ASTNode* tail, const std::string& idName,
                                   Scope& scope,
                                   std::vector<std::string>& diag) {
    const SymbolEntry* entry = scope.lookup(idName);
    if (!entry || !entry->defined) {
        diag.push_back("undefined variable '" + idName + "' in expression");
        return {};
    }

    EvalResult val;
    val.type = entry->type;
    val.intVal = entry->value;
    val.floatVal = (double)entry->value;

    ASTNode* tprime = nullptr;
    ASTNode* cprime = nullptr;
    ASTNode* eprime = nullptr;

    for (auto* c : tail->children) {
        if (c->sym == "Term'")  tprime = c;
        if (c->sym == "Comp'")  cprime = c;
        if (c->sym == "Expr'")  eprime = c;
    }

    while (tprime && !tprime->children.empty()) {
        if (tprime->children[0]->sym == "MUL") {
            EvalResult rhs = evalPower(tprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            VarType restype = resultType(val.type, rhs.type, "*");
            if (restype == VarType::TY_FLOAT) {
                val = promoteToFloat(val);
                rhs = promoteToFloat(rhs);
                val.floatVal *= rhs.floatVal;
                val.intVal = (int)val.floatVal;
            } else {
                val.intVal *= rhs.intVal;
                val.floatVal = (double)val.intVal;
            }
            val.type = restype;
        } else if (tprime->children[0]->sym == "DIV") {
            EvalResult rhs = evalPower(tprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            if (rhs.floatVal == 0.0) { diag.push_back("division by zero"); return {}; }
            VarType restype = (val.type == VarType::TY_FLOAT || rhs.type == VarType::TY_FLOAT)
                                  ? VarType::TY_FLOAT : VarType::TY_INT;
            val = promoteToFloat(val);
            val.floatVal /= rhs.floatVal;
            val.intVal = (int)val.floatVal;
            val.type = restype;
        } else break;
        tprime = (tprime->children.size() >= 3) ? tprime->children[2] : nullptr;
    }

    while (cprime && !cprime->children.empty()) {
        if (cprime->children[0]->sym == "PLUS") {
            EvalResult rhs = evalTerm(cprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            VarType restype = resultType(val.type, rhs.type, "+");
            if (restype == VarType::TY_FLOAT) {
                val = promoteToFloat(val);
                rhs = promoteToFloat(rhs);
                val.floatVal += rhs.floatVal;
                val.intVal = (int)val.floatVal;
            } else {
                val.intVal += rhs.intVal;
                val.floatVal = (double)val.intVal;
            }
            val.type = restype;
        } else if (cprime->children[0]->sym == "MINUS") {
            EvalResult rhs = evalTerm(cprime->children[1], scope, diag);
            if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};
            VarType restype = resultType(val.type, rhs.type, "-");
            if (restype == VarType::TY_FLOAT) {
                val = promoteToFloat(val);
                rhs = promoteToFloat(rhs);
                val.floatVal -= rhs.floatVal;
                val.intVal = (int)val.floatVal;
            } else {
                val.intVal -= rhs.intVal;
                val.floatVal = (double)val.intVal;
            }
            val.type = restype;
        } else break;
        cprime = (cprime->children.size() >= 3) ? cprime->children[2] : nullptr;
    }

    while (eprime && !eprime->children.empty()) {
        const std::string& op = eprime->children[0]->sym;
        EvalResult rhs = evalComp(eprime->children[1], scope, diag);
        if (val.type == VarType::TY_ERROR || rhs.type == VarType::TY_ERROR) return {};

        double va = (val.type == VarType::TY_FLOAT) ? val.floatVal : (double)val.intVal;
        double vb = (rhs.type == VarType::TY_FLOAT) ? rhs.floatVal : (double)rhs.intVal;

        bool result = false;
        if (op == "EQ")       result = (va == vb);
        else if (op == "NE")  result = (va != vb);
        else if (op == "LT")  result = (va < vb);
        else if (op == "GT")  result = (va > vb);
        else if (op == "LE")  result = (va <= vb);
        else if (op == "GE")  result = (va >= vb);
        else break;

        val = makeBool(result);
        eprime = (eprime->children.size() >= 3) ? eprime->children[2] : nullptr;
    }

    return val;
}

// ============================================================
// 语义分析核心逻辑
// ============================================================

static void collectUsedVars(ASTNode* node, std::vector<std::string>& vars) {
    if (!node) return;
    if (node->sym == "ID" && node->isToken && !node->lexeme.empty()) {
        vars.push_back(node->lexeme);
    }
    for (auto* c : node->children) collectUsedVars(c, vars);
}

static void collectUsedVars(ASTNode* node, std::set<std::string>& vars) {
    if (!node) return;
    if (node->sym == "ID" && node->isToken && !node->lexeme.empty()) {
        vars.insert(node->lexeme);
    }
    for (auto* c : node->children) collectUsedVars(c, vars);
}

bool analyzeSemantics(ASTNode* root, SemanticResult& result) {
    result = SemanticResult{};
    if (!root) {
        result.errors.push_back("empty AST");
        return false;
    }

    int nodeIndex = 0;
    std::set<std::string> reportedWarnings;

    auto addWarning = [&](const std::string& key, const std::string& msg) {
        if (reportedWarnings.insert(key).second)
            result.warnings.push_back(msg);
    };

    std::deque<Scope> scopeStack;
    scopeStack.push_back(Scope{}); // global scope

    std::function<void(ASTNode*)> walk = [&](ASTNode* node) {
        if (!node) return;

        if (node->sym == "Stmt") {
            nodeIndex++;

            if (node->children.empty()) { return; }
            const std::string& first = node->children[0]->sym;

            bool isBlock = (first == "LBRACE");
            if (isBlock) {
                Scope block;
                block.parent = &scopeStack.back();
                scopeStack.push_back(block);
            }

            Scope& curScope = scopeStack.back();

            if (first == "ID" && node->children.size() >= 2 &&
                node->children[1]->sym == "StmtTail" &&
                !node->children[1]->children.empty() &&
                node->children[1]->children[0]->sym == "ASSIGN") {

                std::string var = node->children[0]->lexeme;
                auto* tail = node->children[1];
                auto* expr = tail->children[1];

                std::vector<std::string> diag;
                EvalResult rhs = evalExprInternal(expr, curScope, diag);

                for (auto& d : diag) {
                    result.errors.push_back("Stmt#" + std::to_string(nodeIndex) + ": " + d);
                }

                SymbolEntry* existing = curScope.lookup(var);
                SymbolEntry* entry;

                if (!existing || !existing->defined) {
                    entry = &curScope.symbols[var];
                    entry->name = var;
                    entry->defined = true;
                    entry->definedLine = nodeIndex;
                    entry->type = (rhs.type != VarType::TY_ERROR) ? rhs.type : VarType::TY_INT;
                    entry->value = rhs.intVal;
                    result.infos.push_back(
                        "defined variable '" + var + "' (type=" + std::string(typeName(entry->type)) +
                        ", value=" + std::to_string(rhs.intVal) + ") at Stmt#" +
                        std::to_string(nodeIndex));
                } else {
                    entry = existing;
                    VarType oldType = entry->type;
                    VarType newType = (rhs.type != VarType::TY_ERROR) ? rhs.type : VarType::TY_INT;

                    if (oldType != VarType::TY_ERROR && newType != VarType::TY_ERROR &&
                        oldType != newType) {
                        result.warnings.push_back(
                            "type mismatch: variable '" + var +
                            "' was " + std::string(typeName(oldType)) +
                            ", reassigned to " + std::string(typeName(newType)) +
                            " at Stmt#" + std::to_string(nodeIndex));
                    }

                    entry->value = rhs.intVal;
                    result.infos.push_back(
                        "reassigned variable '" + var +
                        "' = " + std::to_string(rhs.intVal) + " at Stmt#" +
                        std::to_string(nodeIndex));
                }

                std::vector<std::string> used;
                collectUsedVars(expr, used);
                for (auto& u : used) {
                    SymbolEntry* ue = curScope.lookup(u);
                    if (!ue || !ue->defined) {
                        addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                            "variable '" + u + "' used before assignment at Stmt#" +
                            std::to_string(nodeIndex));
                    } else {
                        ue->used = true;
                        ue->usedLine = nodeIndex;
                    }
                }
            }
            else if (first == "ID") {
                // 函数调用语句: ID ( Args ) ; — 跳过
                if (node->children.size() >= 2 &&
                    node->children[1]->sym == "StmtTail" &&
                    !node->children[1]->children.empty() &&
                    node->children[1]->children[0]->sym == "LPAREN") {
                    // skip builtin call in semantic analysis
                } else {
                std::set<std::string> stmtUsed;
                stmtUsed.insert(node->children[0]->lexeme);
                if (node->children.size() >= 2 && node->children[1]->sym == "StmtTail") {
                    collectUsedVars(node->children[1], stmtUsed);
                }
                for (auto& u : stmtUsed) {
                    SymbolEntry* ue = curScope.lookup(u);
                    if (!ue || !ue->defined) {
                        addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                            "variable '" + u + "' used before assignment at Stmt#" +
                            std::to_string(nodeIndex));
                    } else {
                        ue->used = true;
                        ue->usedLine = nodeIndex;
                        ue->name = u;
                    }
                }

                if (node->children.size() >= 2 && node->children[1]->sym == "StmtTail") {
                    std::vector<std::string> diag;
                    EvalResult val = evalStmtTailExpr(node->children[1],
                                                      node->children[0]->lexeme,
                                                      curScope, diag);
                    for (auto& d : diag)
                        result.errors.push_back("Stmt#" + std::to_string(nodeIndex) + ": " + d);
                    if (val.type != VarType::TY_ERROR) {
                        result.infos.push_back("expr '" + node->children[0]->lexeme +
                                               "...' = " + std::to_string(val.intVal) +
                                               " [type=" + std::string(typeName(val.type)) +
                                               "] at Stmt#" + std::to_string(nodeIndex));
                    }
                }
                }
            }
            else if (first == "IF") {
                if (node->children.size() >= 3 && node->children[2]->sym == "Expr") {
                    std::set<std::string> used;
                    collectUsedVars(node->children[2], used);
                    for (auto& u : used) {
                        SymbolEntry* ue = curScope.lookup(u);
                        if (!ue || !ue->defined) {
                            addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                "variable '" + u + "' used before assignment at Stmt#" +
                                std::to_string(nodeIndex));
                        } else {
                            ue->used = true;
                            ue->usedLine = nodeIndex;
                            ue->name = u;
                        }
                    }

                    std::vector<std::string> diag;
                    EvalResult cond = evalExprInternal(node->children[2], curScope, diag);
                    for (auto& d : diag)
                        result.errors.push_back("Stmt#" + std::to_string(nodeIndex) + ": " + d);
                    if (cond.type != VarType::TY_ERROR && cond.type != VarType::TY_BOOL) {
                        result.infos.push_back("IF condition evaluated to " +
                                               std::string(typeName(cond.type)) +
                                               " (" + (cond.isTrue() ? "true" : "false") +
                                               ") at Stmt#" + std::to_string(nodeIndex));
                    }
                }
            }
            else if (first == "WHILE") {
                if (node->children.size() >= 3 && node->children[2]->sym == "Expr") {
                    std::set<std::string> used;
                    collectUsedVars(node->children[2], used);
                    for (auto& u : used) {
                        SymbolEntry* ue = curScope.lookup(u);
                        if (!ue || !ue->defined) {
                            addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                "variable '" + u + "' used before assignment at Stmt#" +
                                std::to_string(nodeIndex));
                        } else {
                            ue->used = true;
                            ue->usedLine = nodeIndex;
                            ue->name = u;
                        }
                    }

                    std::vector<std::string> diag;
                    EvalResult cond = evalExprInternal(node->children[2], curScope, diag);
                    for (auto& d : diag)
                        result.errors.push_back("Stmt#" + std::to_string(nodeIndex) + ": " + d);
                    if (cond.type != VarType::TY_ERROR && cond.type != VarType::TY_BOOL) {
                        result.infos.push_back("WHILE condition evaluated to " +
                                               std::string(typeName(cond.type)) +
                                               " at Stmt#" + std::to_string(nodeIndex));
                    }
                }
            }
            else if (first == "INT_KW") {
                if (node->children.size() >= 3 && node->children[2]->sym == "DeclRest") {
                    std::string var = node->children[1]->lexeme;
                    ASTNode* declRest = node->children[2];

                    SymbolEntry* entry = &curScope.symbols[var];

                    if (declRest->children.empty()) {
                        int initVal = 0;
                        entry->name = var;
                        entry->defined = true;
                        entry->definedLine = nodeIndex;
                        entry->type = VarType::TY_INT;
                        entry->value = initVal;
                        result.infos.push_back(
                            "defined variable '" + var + "' (type=int, value=" + std::to_string(initVal) + ") at Stmt#" +
                            std::to_string(nodeIndex));
                    } else if (declRest->children[0]->sym == "ASSIGN") {
                        std::vector<std::string> diag;
                        EvalResult rhs = evalExprInternal(declRest->children[1], curScope, diag);
                        for (auto& d : diag)
                            result.errors.push_back("Stmt#" + std::to_string(nodeIndex) + ": " + d);

                        entry->name = var;
                        entry->defined = true;
                        entry->definedLine = nodeIndex;
                        entry->type = (rhs.type != VarType::TY_ERROR) ? rhs.type : VarType::TY_INT;
                        entry->value = rhs.intVal;
                        result.infos.push_back(
                            "defined variable '" + var + "' (type=" + std::string(typeName(entry->type)) +
                            ", value=" + std::to_string(rhs.intVal) + ") at Stmt#" +
                            std::to_string(nodeIndex));

                        std::vector<std::string> used;
                        collectUsedVars(declRest->children[1], used);
                        for (auto& u : used) {
                            SymbolEntry* ue = curScope.lookup(u);
                            if (!ue || !ue->defined) {
                                addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                    "variable '" + u + "' used before assignment at Stmt#" +
                                    std::to_string(nodeIndex));
                            } else {
                                ue->used = true;
                                ue->usedLine = nodeIndex;
                            }
                        }
                    }
                }
            }
            else if (first == "RETURN") {
                if (node->children.size() >= 2 && node->children[1]->sym == "Expr") {
                    std::set<std::string> used;
                    collectUsedVars(node->children[1], used);
                    for (auto& u : used) {
                        SymbolEntry* ue = curScope.lookup(u);
                        if (!ue || !ue->defined) {
                            addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                "variable '" + u + "' used before assignment at Stmt#" +
                                std::to_string(nodeIndex));
                        } else {
                            ue->used = true;
                            ue->usedLine = nodeIndex;
                            ue->name = u;
                        }
                    }

                    std::vector<std::string> diag;
                    EvalResult retVal = evalExprInternal(node->children[1], curScope, diag);
                    for (auto& d : diag)
                        result.errors.push_back("Stmt#" + std::to_string(nodeIndex) + ": " + d);
                    if (retVal.type != VarType::TY_ERROR) {
                        result.infos.push_back("RETURN value = " +
                                               std::to_string(retVal.intVal) +
                                               " [type=" + std::string(typeName(retVal.type)) +
                                               "] at Stmt#" + std::to_string(nodeIndex));
                    }
                }
            }

            for (auto* c : node->children) walk(c);

            if (isBlock) {
                scopeStack.pop_back();
            }
            return;
        }

        for (auto* c : node->children) walk(c);
    };

    walk(root);

    result.globalScope = scopeStack.front();

    std::function<void(Scope&)> checkUnused = [&](Scope& s) {
        for (auto& kv : s.symbols) {
            if (kv.second.defined && !kv.second.used) {
                result.warnings.push_back(
                    "variable '" + kv.first + "' assigned but never used");
            }
        }
    };
    checkUnused(result.globalScope);

    return true;
}
