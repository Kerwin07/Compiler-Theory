// D:\msys64\ucrt64\bin\g++.exe -std=c++17 -O2 -g ll1.cpp token_io.cpp ll_parser.cpp -o ll_parser.exe

#include "ll1.h"
#include "token_io.h"

#include <fstream>
#include <iostream>
#include <stack>
#include <functional>
#include <set>

static bool isNonTerminalSym(const Grammar& g, const std::string& sym) {
    return g.nonterminals.count(sym) != 0;
}

static void writeStep(std::ofstream& out,
                      const std::vector<std::string>& stk,
                      const std::vector<std::string>& input,
                      size_t ip,
                      const std::string& action) {
    out << "STACK: ";
    for (auto& s : stk) out << s << " ";
    out << "\n";

    out << "INPUT: ";
    for (size_t i = ip; i < input.size(); i++) out << input[i] << " ";
    out << "\n";

    out << "ACTION: " << action << "\n";
    out << "---\n";
}

static bool isSyncToken(const std::string& tok) {
    static const std::set<std::string> sync = {
        "SEMI", "RBRACE", "RPAREN",
        "IF", "WHILE", "RETURN", "ID", "LBRACE"
    };
    return sync.count(tok) || tok == END;
}

int main() {
    Grammar g;
    LLTable t;
    if (!loadLLTable("ll_table.txt", g, t)) {
        std::cerr << "failed to read ll_table.txt" << std::endl;
        return 1;
    }

    std::vector<Token> toks;
    if (!readTokenFile("..\\Lexer\\source_token.txt", toks)) {
        std::cerr << "failed to read ..\\Lexer\\source_token.txt" << std::endl;
        return 1;
    }

    // AST node
    struct ASTNode {
        std::string sym;
        std::string lexeme; // for terminals
        std::vector<ASTNode*> children;
        bool isToken;
        ASTNode(const std::string& s, bool tok=false, const std::string& lex="") : sym(s), lexeme(lex), isToken(tok) {}
    };

    // Collector for building nodes when RHS parsed
    struct Collector { std::string parent; int remaining; std::vector<ASTNode*> children; };

    // 只用 type 参与语法分析
    std::vector<std::string> input;
    input.reserve(toks.size() + 1);
    for (auto& tk : toks) {
        if (tk.type == "FAIL") {
            std::cerr << "lexer has FAIL token, stop." << std::endl;
            return 1;
        }
        input.push_back(tk.type);
    }
    input.push_back(END);

    std::ofstream out("parse_steps.txt");
    if (!out.is_open()) {
        std::cerr << "failed to write parse_steps.txt" << std::endl;
        return 1;
    }

    // 分析栈：底部 $，顶部是 back()
    std::vector<std::string> stk;
    stk.push_back(END);
    stk.push_back(g.startSymbol);

    // collectors stack
    std::vector<Collector> collectors;
    ASTNode* root = nullptr;

    size_t ip = 0;
    int errorCount = 0;

    writeStep(out, stk, input, ip, "INIT");

    while (!stk.empty()) {
        std::string X = stk.back();
        std::string a = (ip < input.size() ? input[ip] : std::string(END));

        if (X == END && a == END) {
            writeStep(out, stk, input, ip, "ACCEPT");
            break;
        }

        if (!isNonTerminalSym(g, X) || X == END) {
            if (X == a) {
                ASTNode* leaf = new ASTNode(X, true, toks[ip].lexeme);
                stk.pop_back();
                ip++;
                if (!collectors.empty()) {
                    collectors.back().children.push_back(leaf);
                    collectors.back().remaining -= 1;
                    while (!collectors.empty() && collectors.back().remaining == 0) {
                        Collector c = collectors.back(); collectors.pop_back();
                        ASTNode* p = new ASTNode(c.parent, false);
                        for (auto child : c.children) p->children.push_back(child);
                        if (!collectors.empty()) {
                            collectors.back().children.push_back(p);
                            collectors.back().remaining -= 1;
                        } else {
                            root = p;
                        }
                    }
                } else {
                    root = leaf;
                }
                writeStep(out, stk, input, ip, std::string("MATCH ") + a);
            } else {
                errorCount++;
                writeStep(out, stk, input, ip, std::string("ERROR: expected ") + X + ", got " + a);
                std::cerr << "parse error #" << errorCount << ": expected " << X << ", got " << a
                          << " (lexeme='" << (ip < toks.size() ? toks[ip].lexeme : "") << "')" << std::endl;
                stk.pop_back();
                collectors.clear();
            }
            continue;
        }

        auto rowIt = t.table.find(X);
        if (rowIt == t.table.end() || rowIt->second.find(a) == rowIt->second.end()) {
            errorCount++;
            std::string currLex = (ip < toks.size()) ? toks[ip].lexeme : "$";
            writeStep(out, stk, input, ip, std::string("ERROR: no rule for M[") + X + "," + a + "]");
            std::cerr << "parse error #" << errorCount << ": no rule for M[" << X << "," << a
                      << "] (lexeme='" << currLex << "')" << std::endl;

            if (a == END) {
                std::cerr << "  unexpected end of input, stopping" << std::endl;
                break;
            }

            while (ip < input.size() && !isSyncToken(input[ip]))
                ip++;
            while (!stk.empty() && stk.back() != END) {
                std::string top = stk.back();
                stk.pop_back();
                if (isNonTerminalSym(g, top)) {
                    auto rit = t.table.find(top);
                    if (rit != t.table.end() && rit->second.count(ip < input.size() ? input[ip] : END)) {
                        stk.push_back(top);
                        break;
                    }
                }
                if (top == "SEMI" || top == "RBRACE") {
                    stk.push_back(top);
                    break;
                }
            }
            collectors.clear();
            writeStep(out, stk, input, ip, "PANIC RECOVERY");
            continue;
        }

        int prodIndex = rowIt->second.at(a);
        if (prodIndex < 0 || prodIndex >= (int)g.prods.size()) {
            std::cerr << "table corrupt: prodIndex out of range" << std::endl;
            return 3;
        }

        const auto& pr = g.prods[prodIndex];
        stk.pop_back();

        if (pr.rhs.size() == 1 && pr.rhs[0] == EPS) {
            ASTNode* p = new ASTNode(pr.lhs, false);
            if (!collectors.empty()) {
                collectors.back().children.push_back(p);
                collectors.back().remaining -= 1;
                while (!collectors.empty() && collectors.back().remaining == 0) {
                    Collector c = collectors.back(); collectors.pop_back();
                    ASTNode* pp = new ASTNode(c.parent, false);
                    for (auto child : c.children) pp->children.push_back(child);
                    if (!collectors.empty()) {
                        collectors.back().children.push_back(pp);
                        collectors.back().remaining -= 1;
                    } else {
                        root = pp;
                    }
                }
            } else {
                root = p;
            }
            std::string action = pr.lhs + " ->";
            for (auto& s : pr.rhs) action += " " + s;
            writeStep(out, stk, input, ip, action);
            continue;
        }

        Collector col; col.parent = pr.lhs; col.remaining = (int)pr.rhs.size(); col.children.clear();
        collectors.push_back(col);
        for (int i = (int)pr.rhs.size() - 1; i >= 0; i--) {
            stk.push_back(pr.rhs[i]);
        }

        std::string action = pr.lhs + " ->";
        for (auto& s : pr.rhs) action += " " + s;
        writeStep(out, stk, input, ip, action);
    }

    // write AST to file in simple parenthesized format
    std::function<void(ASTNode*, std::ostream&, int)> dump = [&](ASTNode* n, std::ostream& os, int indent) {
        for (int i=0;i<indent;i++) os << "  ";
        if (!n) { os << "<null>\n"; return; }
        if (n->isToken) os << n->sym << "('" << n->lexeme << "')\n";
        else os << n->sym << "\n";
        for (auto c : n->children) dump(c, os, indent+1);
    };

    std::ofstream astOut("ast.txt");
    if (astOut.is_open()) {
        if (root) dump(root, astOut, 0);
        astOut.close();
    }

    // Also emit AST as JSON for easier consumption by tools
    auto escapeString = [](const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '\\': r += "\\\\"; break;
                case '"': r += "\\\""; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default: r.push_back(c); break;
            }
        }
        return r;
    };

    std::ofstream jsonOut("ast.json");
    if (jsonOut.is_open()) {
        std::function<void(ASTNode*)> writeJson = [&](ASTNode* n) {
            if (!n) { jsonOut << "null"; return; }
            jsonOut << "{";
            jsonOut << "\"sym\":\"" << escapeString(n->sym) << "\"";
            if (n->isToken) {
                jsonOut << ",\"lexeme\":\"" << escapeString(n->lexeme) << "\"";
            }
            if (!n->children.empty()) {
                jsonOut << ",\"children\":[";
                for (size_t i = 0; i < n->children.size(); ++i) {
                    writeJson(n->children[i]);
                    if (i + 1 < n->children.size()) jsonOut << ',';
                }
                jsonOut << "]";
            }
            jsonOut << "}";
        };

        if (root) writeJson(root);
        jsonOut.close();
    }

    if (errorCount > 0) {
        std::cout << "ACCEPT (with " << errorCount << " error(s))" << std::endl;
    } else {
        std::cout << "ACCEPT" << std::endl;
    }

    return 0;
}
