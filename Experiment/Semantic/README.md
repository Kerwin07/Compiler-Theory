# semantic 实验（语义分析）

在 Parser 的输出 `ast.json` 基础上，实现语义分析。

## 语义分析内容

1. **符号表构建**：遍历 AST，收集所有变量定义和引用
2. **类型系统**：支持 int / float / bool / error 四种类型
3. **类型推断**：从字面量和运算结果自动推断变量类型
4. **类型检查**：变量重赋值时检测类型是否匹配，不匹配输出 warning
5. **表达式求值（计算器）**：递归求值 AST 中的表达式子树
6. **使用前检查**：检测在赋值前就被引用的变量
7. **未使用变量检查**：检测已赋值但从未被引用的变量
8. **重复赋值检查**：同一变量被多次赋值的提示

## 运算符支持

| 运算符 | 优先级 | 说明 |
|--------|--------|------|
| `^` | 最高 | 指数（Power） |
| `*` `/` | 高 | 乘除 |
| `+` `-` | 中 | 加减 |
| `==` | 低 | 相等比较，结果为 bool |

## 类型规则

| 表达式 | 结果类型 |
|--------|----------|
| `INT` 字面量 | int |
| `int op int` (加减乘) | int |
| `int op int` (除法) | float |
| `int / float` 或 `float / int` | float |
| `==` 比较 | bool |
| `^` 指数 | int（底数指数均为 int 时）否则 float |

## 输入/输出

- 输入：`../Parser/ast.json`（语法分析器输出的 AST）
- 输出：
  - 终端：Info / Warnings / Errors + 带类型和值的符号表
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
