# Semantic — 语义分析器

在 Parser 输出的 `ast.json` 基础上执行语义分析，包括类型检查、嵌套作用域管理、表达式求值、语义规则检查。

## 程序

| 程序 | 输入 | 输出 |
|------|------|------|
| `semantic_runner.exe` | `ast.json`（来自 Parser） | 终端输出 + `semantic_report.txt` |

## 分析内容

| 检查项 | 说明 |
|--------|------|
| **符号表管理** | 遍历 AST 收集变量定义/引用信息，支持嵌套块作用域 |
| **作用域链查找** | 沿 Scope.parent 链向上搜索，实现变量遮蔽（shadowing） |
| **类型系统** | int / float / bool / error 四种类型，类型提升（int→float） |
| **类型推导** | 从字面量和运算结果自动推断变量类型 |
| **类型检查** | 重赋值时检测类型不匹配，输出 warning |
| **表达式求值** | 5 层递归求值 AST 表达式子树（编译期常量和符号求值） |
| **use-before-def** | 检测"使用前未定义"的变量，输出 warning |
| **未使用变量** | 检测"已定义但从未被引用"的变量，输出 warning |

## 作用域规则

```
int x = 5;              ← 函数体全局作用域：x = 5
if (x == 5) {
    int r = x + 1;      ← 块作用域：可访问外层 x，r 仅在块内可见
    x = 10;             ← 沿作用域链找到外层 x，重新赋值
}
// 离开块后 r 不可见（Scope 已弹出）
int y = x;              ← x = 10（块内修改已生效）
```

## 运算符支持

| 运算符 | 优先级 | 结果类型 | 说明 |
|--------|--------|---------|------|
| `^` | 最高 | int/float | 指数，右结合 |
| `-` (一元) | | 保持原类型 | 取负 |
| `*` `/` | 高 | int/float（除→float） | 乘除 |
| `+` `-` | 中 | int/float | 加减 |
| `==` `!=` `<` `>` `<=` `>=` | 低 | **bool** | 比较 |

## 类型规则

| 表达式 | 结果类型 |
|--------|---------|
| INT 字面量 | int |
| FLOAT 字面量 | float |
| STRING 字面量 | int（占位） |
| int (±*/^) int | int |
| 含 float 的混合运算 | float |
| 除法（任意类型） | float |
| 比较运算 | bool |
| 一元负号 | 保持原类型 |

## 输入 / 输出

- **输入**：`../Parser/ast.json`
- **输出**：
  - **终端**：Info / Warnings / Errors + 符号表
  - **`semantic_report.txt`**：完整报告

## 编译

```bash
cd Semantic
g++ -std=c++17 -O2 semantic.cpp semantic_runner.cpp -o semantic_runner.exe
```

## 单独运行

```bash
cd Semantic
.\semantic_runner.exe
```

## 输出解读

```
===== Semantic Analysis Report =====

-- Info (N) --                          ← 分析过程信息
  defined variable 'a' (type=int, value=10) at Stmt#3
  defined variable 'c' (type=int, value=16) at Stmt#5
  RETURN value = 0 [type=int] at Stmt#20

-- Warnings --                          ← 语义警告（不阻止运行）
  variable 'z' used before assignment at Stmt#5
  type mismatch: variable 'x' was int, reassigned to bool at Stmt#7

-- Errors --                            ← 语义错误
  Stmt#7: division by zero

-- Symbol Table --                      ← 最终全局符号表
  a  type=int  value=10   defined=Y  used=Y  definedAt=#3  usedAt=#5
  b  type=int  value=3    defined=Y  used=Y  definedAt=#4  usedAt=#5
```
