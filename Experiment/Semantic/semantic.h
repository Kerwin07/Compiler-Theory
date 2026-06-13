#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

struct SymbolEntry {
    std::string name;
    int definedLine;     // AST中首次赋值的位置（用符号序数近似）
    int usedLine;        // 最后一次引用的位置
    bool defined = false;
    bool used = false;
};

struct Scope {
    std::map<std::string, SymbolEntry> symbols;
    Scope* parent = nullptr;
};

struct SemanticResult {
    std::vector<std::string> errors;   // 语义错误（必须修复）
    std::vector<std::string> warnings; // 语义警告
    std::vector<std::string> infos;    // 信息提示

    Scope globalScope;

    bool hasErrors() const { return !errors.empty(); }
};

// AST节点，从ast.json读取
struct ASTNode {
    std::string sym;       // 终结符/非终结符名称
    std::string lexeme;    // 终结符的原始文本
    std::vector<ASTNode*> children;
    bool isToken = false;

    ~ASTNode() { for (auto* c : children) delete c; }
};

// 从ast.json读取AST
ASTNode* loadAST(const std::string& path);

// 执行语义分析
bool analyzeSemantics(ASTNode* root, SemanticResult& result);
