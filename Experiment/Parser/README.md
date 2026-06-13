# parser 实验（LL(1)）

本实验在 lexer 的输出 `source_token.txt` 基础上，实现：

- 程序1：CFG → LL(1) 分析表生成器
- 程序2：LL(1) 语法分析器（基于分析栈 + 分析表）

> 约定：本实验不做行号/列号；Token 只要能区分类型即可。

## 输入/输出文件

### 词法分析器输出（作为 Parser 输入）
- `../Lexer/source_token.txt`
  - 每行：`TOKEN_TYPE\tLEXEME`
  - Parser 默认只使用 `TOKEN_TYPE` 做语法分析（LEXEME 仅用于展示/调试）。

### 程序1：分析表生成器
- 输入文法：`grammar_cfg.txt`
  - 格式：每行一个产生式：
    - `A -> B c D | eps`
  - 约定：
    - 非终结符：以大写字母开头的单词（如 `E`, `Stmt`, `Expr`, `Power`）
    - 终结符：其它符号（如 `ID`, `INT`, `+`, `(`, `)`, `^`）
    - 空串：`eps`
    - 输入结束符：`$`（程序自动加入）

- 输出：`ll_table.txt`
  - 包含：终结符/非终结符集合、产生式编号、FIRST/FOLLOW、预测分析表。

### 程序2：LL(1) 语法分析器
- 输入1：Token 序列：`../Lexer/source_token.txt`
- 输入2：分析表：`ll_table.txt`
- 输出：`parse_steps.txt`
  - 记录推导/匹配过程（栈、剩余输入、动作）。

## 文法则

```
S        → StmtList
Stmt     → ID StmtTail SEMI
         | IF LPAREN Expr RPAREN Stmt ElseOpt
         | WHILE LPAREN Expr RPAREN Stmt
         | LBRACE StmtList RBRACE
         | RETURN Expr SEMI
         | SEMI
Expr     → Comp Expr'          # == 最低
Comp     → Term Comp'          # + -
Term     → Power Term'         # * /
Power    → Factor Power'       # ^ 最高
Power'   → POW Factor Power' | eps
Factor   → LPAREN Expr RPAREN | ID | INT
```

运算符优先级：`== < + - < * / < ^`

## 编译

在 `Parser/` 目录下（MSYS2 ucrt64 g++ 示例）：

- `ll_table_builder.exe`：从 `grammar_cfg.txt` 生成 `ll_table.txt`
- `ll_parser.exe`：从 `ll_table.txt` + `../Lexer/source_token.txt` 进行分析，输出 `parse_steps.txt`
