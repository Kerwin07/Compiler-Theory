# 简易编译器

一个从零实现的编译前端 + 解释执行后端，支持类 C 语法的子集。

## 快速开始

```powershell
# 修改源码
notepad Lexer\source_char.txt

# 一键编译运行
.\build_and_run.ps1
```

## 支持的语言特性

```c
// 变量赋值
x = 5;
y = 10;

// 表达式（运算符优先级：== < + - < * /）
z = x + y * 2;

// 条件分支（可嵌套，else 绑定最近 if）
if (x == 5) {
    z = z + 1;
} else {
    z = 0;
}

// 循环
while (x == 5) {
    x = x + 1;
}

// 返回
return z;
```

## 总体流水线

```
source_char.txt        grammer.txt          grammar_cfg.txt
       │                    │                     │
       ▼                    ▼                     ▼
   ┌───────┐         ┌────────────┐        ┌───────────────┐
   │ Lexer │◄────────│ DFA Builder │        │ LL Table      │
   │ 词法  │         │   (离线)     │        │ Builder (离线) │
   │ 分析  │         └────────────┘        └───────┬───────┘
   └───┬───┘                                      │
       │ source_token.txt                 ll_table.txt
       │          │                              │
       ▼          └──────────┬───────────────────┘
   ┌────────┐                ▼
   │ Parser │──────────▶ ast.json
   │ 语法   │             /    \
   │ 分析   │            /      \
   └────────┘           ▼        ▼
                   ┌────────┐ ┌────────┐
                   │Semantic│ │Codegen │
                   │ 语义   │ │ 代码   │
                   │ 分析   │ │ 执行   │
                   └────────┘ └────────┘
```

---

## 一、词法分析 (Lexer/)

**任务**：将字符流切分为 Token 序列。

### 内部流程

```
正则规则(grammer.txt) → NFA → DFA → 最小化 DFA(mindfa)
                                           │
                               source_char.txt
                                           │
                                           ▼
                                     Token 序列
```

### 三步程序

| 程序 | 输入 | 输出 | 做了什么 |
|------|------|------|----------|
| `dfa_builder.exe` | `grammer.txt` | `dfa` | 正则 → 后缀式(Shunting-yard) → Thompson 构造 NFA → 子集构造 DFA |
| `dfa_minimizer.exe` | `dfa` | `mindfa` | Hopcroft 算法合并等价状态，得到最小化 DFA |
| `lexer_runner.exe` | `mindfa` + `source_char.txt` | `source_token.txt` | 逐个字符推进 DFA，贪心匹配最长 Token |

### 关键算法

- **正则转 NFA**：Thompson 构造法，实现于 `regex_nfa_dfa.cpp`
  - 连接：两个 NFA 首尾 ε-连接
  - 选择 `|`：新起点 ε-分支到两个 NFA
  - 闭包 `*`：起点 ε-跳到原 NFA 起点 + 原 NFA 终点 ε-回到起点
  - 字符类 `[a-z]`：展开为 `|` 链
- **NFA 转 DFA**：子集构造法（ε-闭包 + 状态集映射）
- **DFA 最小化**：Hopcroft 算法，先按"接受/非接受"分区，再逐步细化
- **Token 匹配**：最长匹配策略 —— 沿着 DFA 一直走到无法继续，回退到最后一次到达接受态的位置，输出对应 Token

### Token 规则 (grammer.txt)

```
IF      if         # 关键字（排在 ID 前面，优先匹配）
ID      [A-Za-z_][A-Za-z0-9_]*  # 标识符
INT     [0-9][0-9]*      # 整数
PLUS    \+               # +
EQ      ==               # ==
...
```

---

## 二、语法分析 (Parser/)

**任务**：将 Token 序列按文法规约为抽象语法树 (AST)。

### 内部流程

```
文法(grammar_cfg.txt) → FIRST/FOLLOW 集 → LL(1) 预测分析表(ll_table.txt)
                                                  │
                                          source_token.txt
                                                  │
                                                  ▼
                                          LL(1) 预测分析
                                                  │
                                                  ▼
                                           AST (ast.json)
```

### 两步程序

| 程序 | 输入 | 输出 | 做了什么 |
|------|------|------|----------|
| `ll_table_builder.exe` | `grammar_cfg.txt` | `ll_table.txt` | 计算 FIRST/FOLLOW 集，构造预测分析表 M[A, a] |
| `ll_parser.exe` | `ll_table.txt` + `source_token.txt` | `ast.json` `parse_steps.txt` | 栈驱动的 LL(1) 分析，归约时构造 AST 节点 |

### 关键算法

- **FIRST 集**：不动点迭代，对每个产生式 A→α，FIRST(α) 中的终结符加入 FIRST(A)，若 α⇒*ε 则也加入 ε
- **FOLLOW 集**：不动点迭代，若 A→αBβ，则 FIRST(β)-{ε} 加入 FOLLOW(B)；若 β⇒*ε，则 FOLLOW(A) 加入 FOLLOW(B)
- **预测分析表**：M[A, a] = 产生式 A→α 的编号，对于 a ∈ FIRST(α)
  - 若 α⇒*ε，则对所有 b ∈ FOLLOW(A)，M[A, b] = 该产生式
  - 冲突消解：悬空 else 问题通过两趟填表解决，优先保留非 ε 产生式
- **LL(1) 分析**：栈顶 X 与当前输入 a
  - X = a = $ → 接受
  - X 是终结符 → 匹配弹出，输入指针前进
  - X 是非终结符 → 查表 M[X, a]，弹出 X，逆序压入产生式右部
  - 查表失败 → 报错

### AST 构造

分析过程中维护一个 Collector 栈：展开产生式时压入，子节点收集完毕后弹出并创建父节点。最终输出 JSON 格式：

```json
{"sym":"S","children":[{"sym":"StmtList","children":[...]}]}
```

### 文法 (grammar_cfg.txt)

```
S        → StmtList
Stmt     → ID StmtTail SEMI
         | IF LPAREN Expr RPAREN Stmt ElseOpt
         | WHILE LPAREN Expr RPAREN Stmt
         | LBRACE StmtList RBRACE
         | RETURN Expr SEMI
         | SEMI
Expr     → Comp Expr'          # == 最低优先级
Comp     → Term Comp'          # + -
Term     → Factor Term'        # * /
Factor   → LPAREN Expr RPAREN | ID | INT
```

通过为每种优先级引入独立的非终结符（Expr/Comp/Term/Factor），在文法层面编码了运算符优先级。

---

## 三、语义分析 (Semantic/)

**任务**：检查程序的语义合法性，构建符号表。

### 实现

| 程序 | 输入 | 输出 |
|------|------|------|
| `semantic_runner.exe` | `ast.json` | `semantic_report.txt` |

### 分析内容

遍历 AST 的每个 Stmt 节点，按语句类型分别处理：

| 语句类型 | 处理方式 |
|----------|----------|
| `ID = Expr` | 记录变量定义/重新赋值，检查 RHS 中引用的变量是否已定义 |
| `ID ...` (表达式语句) | 收集变量引用，检查是否已定义 |
| `IF (cond) ...` | 仅检查条件表达式中的变量引用 |
| `WHILE (cond) ...` | 仅检查条件表达式中的变量引用 |
| `RETURN expr` | 检查返回值表达式中的变量引用 |

最终检查：已定义但从未被使用的变量。

### 输出示例

```
-- Info (5) --
  defined variable 'x' at Stmt#1
  defined variable 'y' at Stmt#2
  defined variable 'z' at Stmt#5
  reassigned variable 'z' at Stmt#6
  reassigned variable 'x' at Stmt#9

-- Symbol Table --
  x  defined=true  used=true   definedAt=#1  usedAt=#9
  y  defined=true  used=true   definedAt=#2  usedAt=#5
  z  defined=true  used=true   definedAt=#5  usedAt=#10
```

---

## 四、代码生成/执行 (Codegen/)

**任务**：执行 AST，得到程序运行结果。

### 实现方式

树遍历解释器 (Tree-walking Interpreter)：递归遍历 AST，直接求值表达式、执行语句。无需生成中间代码或字节码。

### 核心函数

| 函数 | 作用 |
|------|------|
| `evalFactor` | 处理 `ID`(查变量表) / `INT`(转整数) / `(Expr)` |
| `evalTerm` | 处理 `* /` 链：左边值 ×/÷ 右边 Factor，循环处理 Term' |
| `evalComp` | 处理 `+ -` 链：左边值 +/- 右边 Term，循环处理 Comp' |
| `evalExpr` | 处理 `==` 链：左边值 == 右边 Comp，返回 1/0，循环处理 Expr' |
| `execStmt` | 按语句类型分发：赋值/if/while/return/块 |
| `execStmtList` | 顺序执行 Stmt 链，遇到 return 则停止 |

### 执行机制

- **环境 (Env)**：`std::map<string, int>`，变量名 → 值
- **条件分支**：`if (cond) ...` —— cond≠0 为真，cond==0 为假
- **循环**：`while (cond) ...` —— 每次迭代前重新求值 cond
- **返回**：`return expr` —— 抛出式返回，值沿递归调用链传回顶层

---

## 项目结构

```
Experiment/
├── build_and_run.ps1      # 一键运行全部
├── Lexer/                  # 词法分析
│   ├── grammer.txt         # token 正则规则
│   ├── source_char.txt     # ★ 源代码（修改这个！）
│   ├── source_token.txt    # 词法分析输出
│   ├── dfa_builder.exe     # 正则→DFA
│   ├── dfa_minimizer.exe   # DFA 最小化
│   └── lexer_runner.exe    # 运行词法分析
├── Parser/                 # 语法分析
│   ├── grammar_cfg.txt     # CFG 文法
│   ├── ll_table.txt        # LL(1) 预测分析表
│   ├── ast.json            # 抽象语法树
│   ├── parse_steps.txt     # 分析步骤
│   ├── ll_table_builder.exe# 文法→分析表
│   └── ll_parser.exe       # 运行语法分析
├── Semantic/               # 语义分析
│   ├── semantic_runner.exe # 运行语义分析
│   └── semantic_report.txt # 分析报告
└── Codegen/                # 代码执行
    ├── codegen_runner.exe  # 运行程序
    └── execution_trace.txt # 执行轨迹
```
