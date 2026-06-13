#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include <cmath>

enum class VarType {
    TY_INT,
    TY_FLOAT,
    TY_BOOL,
    TY_ERROR
};

static inline const char* typeName(VarType t) {
    switch (t) {
        case VarType::TY_INT:   return "int";
        case VarType::TY_FLOAT: return "float";
        case VarType::TY_BOOL:  return "bool";
        case VarType::TY_ERROR: return "error";
        default: return "?";
    }
}

struct EvalResult {
    VarType type = VarType::TY_ERROR;
    int     intVal = 0;
    double  floatVal = 0.0;

    bool isTrue() const {
        if (type == VarType::TY_BOOL) return intVal != 0;
        if (type == VarType::TY_INT)  return intVal != 0;
        if (type == VarType::TY_FLOAT) return floatVal != 0.0;
        return false;
    }
};

struct SymbolEntry {
    std::string name;
    int definedLine = 0;
    int usedLine = 0;
    bool defined = false;
    bool used = false;

    VarType type = VarType::TY_ERROR;
    int     value = 0;
};

struct Scope {
    std::map<std::string, SymbolEntry> symbols;
    Scope* parent = nullptr;
};

struct SemanticResult {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> infos;

    Scope globalScope;

    bool hasErrors() const { return !errors.empty(); }
};

struct ASTNode {
    std::string sym;
    std::string lexeme;
    std::vector<ASTNode*> children;
    bool isToken = false;

    ~ASTNode() { for (auto* c : children) delete c; }
};

ASTNode* loadAST(const std::string& path);

bool analyzeSemantics(ASTNode* root, SemanticResult& result);

EvalResult evalExpr(ASTNode* expr, const std::map<std::string, SymbolEntry>& symbols,
                    std::vector<std::string>& diag);
