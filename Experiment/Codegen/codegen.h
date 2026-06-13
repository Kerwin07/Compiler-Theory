#pragma once

#include <map>
#include <string>
#include <vector>

struct ASTNode {
    std::string sym;
    std::string lexeme;
    std::vector<ASTNode*> children;
    bool isToken = false;

    ~ASTNode() { for (auto* c : children) delete c; }
};

ASTNode* loadAST(const std::string& path);

struct Env {
    std::map<std::string, int> vars;

    int  get(const std::string& name) const;
    void set(const std::string& name, int val);
};

struct ExecResult {
    bool  hasReturn = false;
    int   returnValue = 0;
};

// 执行整个AST，返回最终return值
int executeProgram(ASTNode* root);
