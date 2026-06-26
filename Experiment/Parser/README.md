# Parser — 语法分析器 (LL(1))

在 Lexer 输出的 `source_token.txt` 基础上，按 CFG 文法执行 LL(1) 预测分析，同步构造 AST。

## 两步程序

| 步骤 | 程序 | 输入 | 输出 | 作用 |
|------|------|------|------|------|
| 1 (离线) | `ll_table_builder.exe` | `grammar_cfg.txt` | `ll_table.txt` | 计算 FIRST/FOLLOW，构建 LL(1) 预测分析表 |
| 2 (运行时) | `ll_parser.exe` | `ll_table.txt` + `source_token.txt` | `ast.json`, `ast.txt`, `parse_steps.txt` | 栈驱动 LL(1) 分析 + Collector 栈 AST 构造 + Panic Recovery |

## 输入文件

- **`grammar_cfg.txt`**：CFG 文法（49 条产生式，消除左递归，优先级编码在文法层次中）
- **`source_token.txt`**：词法分析器输出的 Token 序列（由 Lexer 生成）

## 输出文件

- **`ast.json`**：抽象语法树（JSON 格式）→ 供 Semantic 和 Codegen 使用
- **`ast.txt`**：抽象语法树（缩进可读文本）
- **`parse_steps.txt`**：推导过程（栈状态、剩余输入、每步动作）→ 调试用
- **`ll_table.txt`**：LL(1) 预测分析表（含 FIRST/FOLLOW 集）

## 文法（49 条产生式）

```
S → INT_KW ID LPAREN RPAREN Block        ← C 风格函数定义
S → StmtList                               ← 旧风格向后兼容

Block → LBRACE StmtList RBRACE

StmtList → Stmt StmtList | eps

Stmt → INT_KW ID DeclRest SEMI            ← 类型声明
Stmt → ID StmtTail SEMI                   ← 赋值 / 表达式 / 函数调用
Stmt → IF LPAREN Expr RPAREN Stmt ElseOpt
Stmt → WHILE LPAREN Expr RPAREN Stmt
Stmt → LBRACE StmtList RBRACE
Stmt → RETURN Expr SEMI
Stmt → SEMI

DeclRest → ASSIGN Expr | eps

ElseOpt → ELSE Stmt | eps

StmtTail → ASSIGN Expr                    ← 赋值
StmtTail → LPAREN Args RPAREN             ← 函数调用
StmtTail → Term' Comp' Expr'              ← 表达式

Expr → Comp Expr'
Expr' → EQ Comp Expr' | NE Comp Expr' | LT Comp Expr' | GT Comp Expr' | LE Comp Expr' | GE Comp Expr' | eps
Comp → Term Comp'
Comp' → PLUS Term Comp' | MINUS Term Comp' | eps
Term → Power Term'
Term' → MUL Power Term' | DIV Power Term' | eps
Power → Factor PowerRest
PowerRest → POW Power | eps
Factor → LPAREN Expr RPAREN
Factor → ID FactorRest
Factor → INT | STRING
Factor → MINUS Factor

FactorRest → LPAREN Args RPAREN           ← 函数调用在表达式中
FactorRest → eps

Args → Expr ArgTail | eps
ArgTail → COMMA Expr ArgTail | eps
```

**运算符优先级（从低到高）**：`== != < > <= >=` < `+ -` < `* /` < `^` < 一元 `-`

## 关键特性

- **LL(1) 预测分析**：栈驱动、表驱动，O(n) 复杂度
- **AST 单趟同步构造**：Collector 栈机制，归约时自底向上构建
- **Panic Recovery**：语法错误时跳过输入到同步 Token，弹栈恢复，不崩溃
- **悬空 else 消解**：两趟填表策略，else 绑定最近 if
- **多比较运算符**：支持 `==` `!=` `<` `>` `<=` `>=` 六种

## 编译（如需重新生成 exe）

```bash
cd Parser
g++ -std=c++17 -O2 ll_table_builder.cpp ll1.cpp token_io.cpp -o ll_table_builder.exe
g++ -std=c++17 -O2 ll_parser.cpp ll1.cpp token_io.cpp -o ll_parser.exe
```

## 单独运行

```bash
cd Parser
.\ll_table_builder.exe     # 离线：生成 ll_table.txt（仅文法变化时执行）
.\ll_parser.exe            # 运行时：解析 → ast.json + parse_steps.txt
```

## 输出解读

终端输出：`ACCEPT`（成功）或 `ACCEPT (with N error(s))`（有语法错误但已恢复）

`parse_steps.txt` 示例：
```
STACK: $ S
INPUT: INT_KW ID LPAREN RPAREN LBRACE ...
ACTION: INIT
---
STACK: $ Block RPAREN LPAREN ID INT_KW
INPUT: INT_KW ID LPAREN RPAREN LBRACE ...
ACTION: S -> INT_KW ID LPAREN RPAREN Block
---
STACK: $ Block RPAREN LPAREN ID
INPUT: ID LPAREN RPAREN LBRACE ...
ACTION: MATCH INT_KW
---
```
