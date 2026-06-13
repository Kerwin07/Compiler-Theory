# codegen 实验（代码生成 / 解释执行）

在 Parser 的输出 `ast.json` 基础上，实现树遍历解释器。

## 实现方式

树遍历解释器（Tree-walking Interpreter）：
- 递归遍历 AST，直接求值表达式、执行语句
- 利用 AST 结构中已编码的运算符优先级
- 维护变量环境（Env: name → value）

## 支持的语句

- `ID = Expr;` — 赋值
- `ID + Expr;` / `ID * Expr;` 等 — 表达式语句
- `if (Expr) Stmt [else Stmt]` — 条件分支
- `while (Expr) Stmt` — 循环
- `{ StmtList }` — 语句块
- `return Expr;` — 返回
- `;` — 空语句

## 运算符

| 优先级 | 运算符 |
|--------|--------|
| 高 | ^ (指数) |
|    | \* / |
| 中 | + - |
| 低 | == |

条件中 0 为假，非 0 为真。

## 输入/输出

- 输入：`../Parser/ast.json`
- 输出：
  - 终端：最终返回值
  - `execution_trace.txt`：完整执行轨迹

## 编译

```
g++ -std=c++17 -O2 -g codegen.cpp codegen_runner.cpp -o codegen_runner.exe
```

## 完整流水线

```
source_char.txt ──▶ Lexer   ──▶ source_token.txt
grammer.txt                            │
                                       ▼
grammar_cfg.txt              Parser ──▶ ast.json
    │                         /  \
    ▼                        /    \
ll_table_builder.exe        /      \
                    Semantic       Codegen
                       │              │
                       ▼              ▼
              semantic_report.txt  execution_trace.txt
```
