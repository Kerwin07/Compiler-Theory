#pragma once

#include <string>
#include <vector>

struct Token {
    std::string type;   // 例如：ID、INT、PLUS、IF...
    std::string lexeme; // 原始文本（可选使用）
};

// 从 lexer 输出的 source_token.txt 读取 token 序列。
// 格式：每行 TOKEN_TYPE\tLEXEME
// 解析失败会返回 false。
bool readTokenFile(const std::string& path, std::vector<Token>& outTokens);
