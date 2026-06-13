# semantic 实验（语义分析）

在 Parser 的输出 `ast.json` 基础上，实现语义分析。

## 语义分析内容

1. **符号表构建**：遍历 AST，收集所有变量定义和引用
2. **使用前检查**：检测在赋值前就被引用的变量
3. **未使用变量检查**：检测已赋值但从未被引用的变量
4. **重复赋值检查**：同一变量被多次赋值的警告

## 输入/输出

- 输入：`../Parser/ast.json`（语法分析器输出的 AST）
- 输出：
  - 终端：Info / Warnings / Errors + 符号表
  - `semantic_report.txt`：完整的语义分析报告

## 编译

在 `Semantic/` 目录下（MSYS2 ucrt64 g++）：

```
g++ -std=c++17 -O2 -g semantic.cpp semantic_runner.cpp -o semantic_runner.exe
```

## 完整流水线

```
grammer.txt ──▶ Lexer   ──▶ source_token.txt
                            │
grammar_cfg.txt             │
    │                       ▼
    ▼              Parser ──▶ ast.json
ll_table_builder.exe              │
    │                              ▼
    ▼              Semantic ──▶ semantic_report.txt
ll_parser.exe
```
