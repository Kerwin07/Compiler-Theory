#include "semantic.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <functional>

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
    i++; // skip closing "
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

// 递归解析AST节点
static ASTNode* parseNode(const std::string& js, size_t& i) {
    skipWhitespace(js, i);
    if (i >= js.size()) return nullptr;

    if (js[i] == 'n') { // null
        i += 4;
        return nullptr;
    }

    if (js[i] != '{') return nullptr;
    i++; // skip {

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
// 语义分析核心逻辑
// ============================================================

// 找出Stmt中定义的变量名（ID StmtTail 中 StmtTail -> ASSIGN Expr）
static std::string getDefinedVar(ASTNode* stmtNode) {
    if (stmtNode->sym != "Stmt") return "";
    if (stmtNode->children.size() < 3) return "";
    if (stmtNode->children[0]->sym != "ID") return "";

    auto* tail = stmtNode->children[1];
    if (tail->sym != "StmtTail") return "";
    if (tail->children.empty()) return "";
    if (tail->children[0]->sym == "ASSIGN") {
        return stmtNode->children[0]->lexeme;
    }
    return "";
}

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

// 主分析函数
bool analyzeSemantics(ASTNode* root, SemanticResult& result) {
    result = SemanticResult{};
    if (!root) {
        result.errors.push_back("empty AST");
        return false;
    }

    int nodeIndex = 0;
    std::set<std::string> reportedWarnings; // 去重

    auto addWarning = [&](const std::string& key, const std::string& msg) {
        if (reportedWarnings.insert(key).second)
            result.warnings.push_back(msg);
    };

    std::function<void(ASTNode*)> walk = [&](ASTNode* node) {
        if (!node) return;

        if (node->sym == "Stmt") {
            nodeIndex++;

            // 判断语句类型
            if (node->children.empty()) { return; }
            const std::string& first = node->children[0]->sym;

            // 赋值语句: ID StmtTail SEMI, StmtTail -> ASSIGN Expr
            if (first == "ID" && node->children.size() >= 2 &&
                node->children[1]->sym == "StmtTail" &&
                !node->children[1]->children.empty() &&
                node->children[1]->children[0]->sym == "ASSIGN") {

                std::string var = node->children[0]->lexeme;
                auto* entry = &result.globalScope.symbols[var];

                if (!entry->defined) {
                    entry->name = var;
                    entry->defined = true;
                    entry->definedLine = nodeIndex;
                    result.infos.push_back(
                        "defined variable '" + var + "' at Stmt#" +
                        std::to_string(nodeIndex));
                } else {
                    result.infos.push_back(
                        "reassigned variable '" + var + "' at Stmt#" +
                        std::to_string(nodeIndex));
                }

                // 检查 RHS 中的变量引用
                auto* tail = node->children[1];
                for (auto* tc : tail->children) {
                    if (tc->sym == "Expr") {
                        std::vector<std::string> used;
                        collectUsedVars(tc, used);
                        for (auto& u : used) {
                            auto* ue = &result.globalScope.symbols[u];
                            if (!ue->defined) {
                                addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                    "variable '" + u + "' used before assignment at Stmt#" +
                                    std::to_string(nodeIndex));
                            }
                            ue->used = true;
                            ue->usedLine = nodeIndex;
                        }
                    }
                }
            }
            // 表达式语句: ID StmtTail (无赋值)
            else if (first == "ID") {
                std::set<std::string> stmtUsed;
                stmtUsed.insert(node->children[0]->lexeme);
                // 仅收集 StmtTail 中的引用（不含子语句）
                if (node->children.size() >= 2 && node->children[1]->sym == "StmtTail") {
                    collectUsedVars(node->children[1], stmtUsed);
                }
                for (auto& u : stmtUsed) {
                    auto* entry = &result.globalScope.symbols[u];
                    if (!entry->defined) {
                        addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                            "variable '" + u + "' used before assignment at Stmt#" +
                            std::to_string(nodeIndex));
                    }
                    entry->used = true;
                    entry->usedLine = nodeIndex;
                    entry->name = u;
                }
            }
            // IF: 只检查条件中的引用
            else if (first == "IF") {
                if (node->children.size() >= 3 && node->children[2]->sym == "Expr") {
                    std::set<std::string> used;
                    collectUsedVars(node->children[2], used);
                    for (auto& u : used) {
                        auto* ue = &result.globalScope.symbols[u];
                        if (!ue->defined) {
                            addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                "variable '" + u + "' used before assignment at Stmt#" +
                                std::to_string(nodeIndex));
                        }
                        ue->used = true;
                        ue->usedLine = nodeIndex;
                        ue->name = u;
                    }
                }
            }
            // WHILE: 只检查条件中的引用
            else if (first == "WHILE") {
                if (node->children.size() >= 3 && node->children[2]->sym == "Expr") {
                    std::set<std::string> used;
                    collectUsedVars(node->children[2], used);
                    for (auto& u : used) {
                        auto* ue = &result.globalScope.symbols[u];
                        if (!ue->defined) {
                            addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                "variable '" + u + "' used before assignment at Stmt#" +
                                std::to_string(nodeIndex));
                        }
                        ue->used = true;
                        ue->usedLine = nodeIndex;
                        ue->name = u;
                    }
                }
            }
            // RETURN: 检查返回值表达式
            else if (first == "RETURN") {
                if (node->children.size() >= 2 && node->children[1]->sym == "Expr") {
                    std::set<std::string> used;
                    collectUsedVars(node->children[1], used);
                    for (auto& u : used) {
                        auto* ue = &result.globalScope.symbols[u];
                        if (!ue->defined) {
                            addWarning("use_before_def_" + u + "@" + std::to_string(nodeIndex),
                                "variable '" + u + "' used before assignment at Stmt#" +
                                std::to_string(nodeIndex));
                        }
                        ue->used = true;
                        ue->usedLine = nodeIndex;
                        ue->name = u;
                    }
                }
            }
            // LBRACE / SEMI: 无需检查
        }

        for (auto* c : node->children) walk(c);
    };

    walk(root);

    // 检查已声明但未使用的变量
    for (auto& kv : result.globalScope.symbols) {
        if (kv.second.defined && !kv.second.used) {
            result.warnings.push_back(
                "variable '" + kv.first + "' assigned but never used");
        }
    }

    return true;
}
