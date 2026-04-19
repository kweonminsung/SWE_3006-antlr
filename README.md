# ANTLR 4 Tutorial & PA#1

## Introduction

- ANTLL (Another Tool for Language Recognition)
- A powerful parser generator
- Parser for reading, processing, executing, or translating structured text or binary files
- Widely used to build languages, tools, and frameworks

### ANTLR
- Input: a grammar file (e.g., `Hello.g4`)
- Output: parser code in Java (e.g., `Hello*.java`)

---

## Parse Tree

- ANTLR-generated parser builds a data structure
- Parse tree (or syntax tree)
- “organization of input” according to grammar

---

## Example Grammar File (`*.g4`)

```antlr
/* Example grammar for Expr.g4 */
grammar Expr; // name of grammar

// parser rules – start with lowercase letters
prog: (expr NEWLINE)* ;

expr
    : expr ('*'|'/') expr
    | expr ('+'|'-') expr
    | INT
    | '(' expr ')'
    ;

// lexer rules – start with uppercase letters
NEWLINE : [\r\n]+ ;
INT : [0-9]+ ;
WS : [ \t\r\n]+ -> skip ;
```

---

## Lexer: Regular Expressions

- `.` matches any single character
- `*` matches zero or more copies of preceding expression
- `+` matches one or more copies of preceding expression
- `?` matches zero or one copy of preceding expression
- `-?[0-9]+` : signed numbers including optional minus sign
- `[ ]` matches any character within the brackets
- `[Abc1]`, `[A-Z]`, `[A-Za-z]`, `[^0-9]`
- `^` matches the beginning of line
- `$` matches the end of line
- `\` escapes a metacharacter, e.g. `\*` matches `*`
- `|` matches either the preceding expression or the following
- `abc|ABC`, `(abc)|(ABC)`, `[abc]|[ABC]`
- `( )` groups a series of regular expressions
- `(123)(123)*`

Any non-digit, i.e., any except `0-9`

---

## Parser: Subrules

- `(x|y|z)` : match any alternative within the subrule exactly
  - `returnType: (type | 'void') ;`

- `(x|y|z)?` : match nothing or any alternative within subrule
  - `classDecl: 'class' ID (typeParameters)? ;`

- `(x|y|z)*` : match an alternative within subrule zero or more times
  - `annotationName: ID ('.' ID)* ;`

- `(x|y|z)+` : match an alternative within subrule one or more times
  - `annotations: (annotation)+ ;`

---

## Running ANTLR Parser Generator (C++ ver.)

- Writing a grammar file
  - e.g., `Expr.g4`
- Process with ANTLR for C++

```bash
$ antlr4 -Dlanguage=Cpp Expr.g4
$ ls *.cpp *.h
ExprBaseListener.cpp  ExprBaseListener.h
ExprListener.cpp      ExprListener.h
ExprLexer.cpp         ExprLexer.h
ExprParser.cpp        ExprParser.h
```

- Now we are ready to write main program

---

## Running ANTLR Parser Generator (Java ver.)

- Writing a grammar file
  - e.g., `Expr.g4`
- Process with ANTLR

```bash
$ antlr4 Expr.g4
```

- Compile Java programs

```bash
$ javac Expr*.java
```

- Run a generated parse

```bash
$ grun Expr prog -gui
$ grun Expr prog -tree
```

Example:

```bash
$ antlr4 Expr.g4
$ javac Expr*.java
$ grun Expr prog -gui
100 + 2*34
^D
(prog (expr (expr 100) + (expr (expr 2) * (expr 34))) \n)
```

---

## Parse Tree Manipulation

- Now, you have a parse tree.
- Walk a parse tree with ANTLR tools – Listener or Visitor

### Listener
- Walk all parse tree with DFS from the first root node
- Make functions triggered at entering/exit of nodes
- e.g., `ExprBaseListener.cpp/h` is generated from `antlr4`

### Visitor
- Make functions triggered at entering/exit of nodes
- Unlike listener, user explicitly calls visitor on child nodes
- To generate visitor class, use `-visitor` option for `antlr4`

```bash
$ antlr4 -Dlanguage=Cpp -no-listener -visitor -o antlr4-cpp B.g4
```

---

## `B.g4`: Parser/Lexer Rules for B Lang.

```antlr
/* B.g4 */
grammar B;

// parser rules
program
    : ( directive | definition )* EOF
    ;

directive
    : SHARP_DIRECTIVE
    ;

definition
    : autostmt
    | declstmt
    | funcdef
    ;

autostmt
    : AUTO name (ASSN constant)? (',' name (ASSN constant)?)* SEMI
    ;

declstmt
    : AUTO name '(' (AUTO (',' AUTO )*)? ')' SEMI
    ;

funcdef
    : AUTO name '(' (AUTO name (',' AUTO name)*)? ')' blockstmt
    ;

blockstmt
    : '{' statement* '}'
    ;

statement
    : autostmt
    | declstmt
    | blockstmt
    | ifstmt
    | whilestmt
    | expressionstmt
    | returnstmt
    | nullstmt
    | directive
    ;

ifstmt
    : IF '(' expr ')' statement (ELSE statement)?
    ;

whilestmt
    : WHILE '(' expr ')' statement
    ;

...

// lexer rules
AUTO   : 'auto' ;
SEMI   : ';' ;
...
IF     : 'if' ;
ELSE   : 'else' ;
WHILE  : 'while' ;
RETURN : 'return' ;
ASSN   : '=' ;
WS     : [ \t\r\n]+ -> skip ;
```

---

## `BBaseVisitor.cpp/h`

```cpp
// Generated from B.g4 by ANTLR 4.13.1
#pragma once
#include "antlr4-runtime.h"
#include "BVisitor.h"

class BBaseVisitor : public BVisitor {
public:
    virtual std::any visitProgram(BParser::ProgramContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitDirective(BParser::DirectiveContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitDefinition(BParser::DefinitionContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitAutostmt(BParser::AutostmtContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitDeclstmt(BParser::DeclstmtContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitFuncdef(BParser::FuncdefContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitBlockstmt(BParser::BlockstmtContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitStatement(BParser::StatementContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitIfstmt(BParser::IfstmtContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitWhilestmt(BParser::WhilestmtContext *ctx) override { return visitChildren(ctx); }
    ...
    virtual std::any visitExpression(BParser::ExpressionContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitFuncinvocation(BParser::FuncinvocationContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitConstant(BParser::ConstantContext *ctx) override { return visitChildren(ctx); }
    virtual std::any visitName(BParser::NameContext *ctx) override { return visitChildren(ctx); }
};
```

`BBaseVisitor.cpp/h` is generated by ANTLR4 along with multiple `.cpp/.h` files and others.

You can find them in `antlr4-cpp/*`.

```bash
$ java -jar /usr/local/lib/antlr-complete.jar -Dlanguage=Cpp -no-listener -visitor -o antlr4-cpp B.g4
$ make antlr
$ ls antlr4-cpp/*
```

---

## Skeleton Code

### Files in skeleton code
- `B2CMain.cpp`
- `B.g4`
- `Makefile`
- `INPUTS/input*.b`

```cpp
/* B2CMain.cpp */
...
class SymbolTableVisitor : public BBaseVisitor {
    // build symbol tables for functions and globals
    // ...
};

class TypeAnalysisVisitor : public BBaseVisitor {
    // infer types for 'auto' variables and functions
    // ...
};

class PrintTreeVisitor : public BBaseVisitor {
    // skeleton code will print out for B language
    // you need to change them to print out right type
    // ...
}

/* B2CMain.cpp – continued */
int main(int argc, const char* argv[]) {
    // ANTLR parsing ...
    ParseTree* tree = parser.program();

    // STEP 1. build symbol table
    SymbolTableVisitor SymtabTree;
    SymtabTree.visit(tree);

    // STEP 2. infer types for 'auto' vars/fns
    TypeAnalysisVisitor AnalyzeTree;
    AnalyzeTree.visit(tree);

    // STEP 3. print out correctly typed C code
    PrintTreeVisitor PrintTree;
    PrintTree.visit(tree);

    return 0;
}
```

---

## `B2CMain.cpp` --- symbol table

```cpp
#include <iostream>
#include "ExprBaseListener.h"
#include "ExprLexer.h"
#include "ExprParser.h"

using namespace std;
using namespace antlr4;
using namespace antlr4::tree;

enum Types { tyAUTO, tyINT, tyDOUBLE, tySTRING, tyBOOL, tyCHAR, tyFUNCTION };
string mnemonicTypes[] = { "auto", "int", "double", "string", "bool", "char", "function" };

struct SymbolAttributes {
    Types type; // int, double, bool, char, string, function --- auto if unknown yet

    // if type == "function"
    vector<Types> retArgTypes; // first element is a return_type
};

class SymbolTable {
private:
    map<string, SymbolAttributes> table; // symbol-name: string, symbol-typeInfo: SymbolAttributes

public:
    // Add a new symbol
    void addSymbol(const string& name, const SymbolAttributes& attributes) {
        table[name] = attributes;
    }

    ...

    // Print all symbols in the table (for debugging purposes)
    void printSymbols() const {
        for (const auto& pair : table) {
            cout << "(name) " << pair.first << ", (type) " << mnemonicTypes[pair.second.type];
            ...
        }
    }
};
```

Example:

```cpp
( a, { tyAUTO } )
( b, { tyAUTO } )
(main, { tyFUNCTION, [auto] })
symTabs[$_global_$]

( i, { tyAUTO } )
( j, { tyAUTO } )
( phi, { tyAUTO } )
symTabs[main]
```

```c
/* input.b */
auto a, b = 10;
auto main() {
    auto i, j, phi = 3.14;
    ...
}
```

---

## `B2CMain.cpp` --- building symbol tables

```cpp
const string _GlobalFuncName_ = "$_global_$";

// collection of per-function symbol tables accessed by function name
// symbol table in global scope can be accessed with special name defined in _GlobalFuncName_
map<string, SymbolTable*> symTabs;

class SymbolTableVisitor : public BBaseVisitor {
private:
    int scopeLevel;
    string curFuncName;

public:
    // Building symbol tables by visiting tree
    any visitProgram(BParser::ProgramContext *ctx) override {
        scopeLevel = 0; // global scope

        // prepare symbol table for global scope
        SymbolTable* globalSymTab = new SymbolTable();
        curFuncName = _GlobalFuncName_;
        symTabs[curFuncName] = globalSymTab;

        // visit children
        for (int i = 0; i < ctx->children.size(); i++) {
            visit(ctx->children[i]);
        }

        // print all symbol tables
        for (auto& pair : symTabs) {
            cout << "--- Symbol Table --- " << pair.first << endl; // function name
            pair.second->printSymbols(); // per-function symbol table
            cout << "";
        }

        return nullptr;
    }
    ...
};
```

```c
/* input.b */
auto a, b = 10;
auto main() {
    auto i, j, phi = 3.14;
    ...
}
```

```bash
$ make
$ ./b2c input.b
--- Symbol Table --- $_global_$
(name) a, (type) auto
(name) b, (type) int
(name) main, (type) function | auto ()
--- Symbol Table --- main
(name) i, (type) auto
(name) j, (type) auto
(name) phi, (type) double
$
```

```bash
$ sudo apt install indent
$ ./b2c input.b | indent
```

(show source code with indentation)

---

## Building symbol table with scope

```c
/* input0.b */
auto a, b = 10;
auto fn(auto, auto);

auto main() {
    auto i, j, phi = 3.14;
    auto a;
    {
        auto x;
    }
    while (true) {
        auto y;
        if (y) {
            auto z;
        } else {
        }
    }
}

auto fn(auto x, auto y) {
}
```

```bash
$ ./b2c input0.b
--- Symbol Table --- $_global_$
(name) a, (type) auto
(name) b, (type) int
(name) fn, (type) function | auto (auto, auto)
(name) main, (type) function | auto ()
--- Symbol Table --- main
(name) i, (type) auto
(name) j, (type) auto
(name) phi, (type) double
(name) a, (type) auto
--- Symbol Table --- main_$1
(name) x, (type) auto
--- Symbol Table --- main_$2
(name) y, (type) auto
--- Symbol Table --- main_$2_1
(name) z, (type) auto
--- Symbol Table --- main_$2_2
--- Symbol Table --- fn
(name) x, (type) auto
(name) y, (type) auto
$
```

---

## Multiple Definition Errors

- Multiple declaration of the same name variable
- Error – if declared at the same depth of scope
- Insert them into different tables – if declared at different depth of scope

- Multiple definition of the same name functions
- Error – in principle, it's an error if defined at the same depth of scope

- `inc(auto a), inc(auto a)` → ERROR

### Unsupported
- Overloaded functions
- Nested functions

These are **not supported** and **will not be included in test cases**.

- `add(auto a), add(auto a, auto b)` → X (NO Support)
- `add() { inc(auto a) { ... } ... }` → X (NO Support)

```c
auto a;

auto main() {
    auto x;
    auto a; // OK
    auto x; // Error
}
```

---

## [PA#1] Submission & Grading Policy

- **[PA #1] Build symbol table**
- Download `B2C.zip` from iCampus Assignment
- Complete **STEP 1** of `B2CMain.cpp`
- Submit `B2CMain.cpp` only — the instructors will use all the other files from `B2C.zip`
- If you need to modify other files, let them know via Q&A board
- Discussion is allowed, but plagiarism is not allowed
- If any code is copied from elsewhere (e.g., friends or internet), you get **0 points**
- This policy applies equally to all later projects as well

---

## Reference

- *The Definitive ANTLR 4 Reference* - Terence Parr
- `http://antlr.org` > Dev Tools > Resources
- Documentation
  - `https://github.com/antlr/antlr4/blob/master/doc/index.md`
- Runtime API (look into “Java Runtime” for ANTLR4 APIs)
  - `http://www.antlr.org/api/`
- Java util package
  - `http://www.tutorialspoint.com/java/util/index.htm`
- Other resource (C++ target)
  - `https://tomassetti.me/getting-started-antlr-cpp/`
  - `http://www.cs.sjsu.edu/~mak/tutorials/InstallANTLR4Cpp.pdf`
- C++ STL tutorials
  - `https://www.studytonight.com/cpp/stl/`
  - `https://www.cppreference.com/Cpp_STL_ReferenceManual.pdf`
