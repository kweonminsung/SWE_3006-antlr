# PA #2 - B2C Translator: Type Inference

This project extends the PA #1 B-to-C translator by adding type inference for the B language. The translator reads B source code, infers every `auto` type, updates the symbol table during parse-tree traversal, and prints correctly typed C/C++-compilable code to `stdout`.

The original assignment material is preserved at the end of this README as rendered slide captures so that both the textual and visual contents of the PDF are included.

## 1. Assignment Overview

- Assignment: **PA #2 - B2C Translator**
- Source language: **B**, a C-like language defined in `B.g4`
- Implementation style: **ANTLR visitor pattern**
- Translator type: **B source code to C/C++ source code**
- Main goal: infer all implicitly typed `auto` variables, function parameters, and function return types
- Required output: C/C++ source code with `auto` replaced by concrete types
- Error policy: stop at the first type error, print the error, and exit

## 2. Supported Types

B source code writes all user-defined types as `auto`, but the translator must infer one of the following actual types:

| Type | Meaning |
| --- | --- |
| `int` | integer value |
| `double` | floating-point value |
| `char` | character value |
| `string` | string value |
| `bool` | boolean value |
| `void` | function return type only |

`void` is only valid as a function return type. It must not be assigned as a variable type.

## 3. Core Type System Rule

The B language is strongly typed.

There are no compatible or implicitly convertible types. Operations are valid only when the relevant operands have exactly the same inferred type. For example, an `int` expression and a `double` expression must not be treated as compatible.

Whenever operations among different types occur, the translator must call `print_error_and_exit`.

## 4. Type Inference Rules

### 4.1 Variable Type Inference

A variable declared with `auto` receives its type from its first assignment or initializer.

```b
auto x;
...
x = alpha;
```

Inference rule:

```text
type(x) := inferred_type(alpha)
```

If the variable is initialized at declaration time, the initializer acts as the first assignment.

```b
auto a = 1;      // a: int
auto f = 1.0;    // f: double
```

After the type is fixed, every later assignment to the same variable must use the same type.

### 4.2 Function Return Type Inference

A function declared with `auto` receives its return type from its `return` statement.

```b
auto func(auto x, auto y) {
    ...
    return gamma;
}
```

Inference rule:

```text
return_type(func) := inferred_type(gamma)
```

If the function body has no `return` statement, the function return type is `void`.

```b
auto func() {
    // no return statement
}
```

Inference result:

```text
return_type(func) == void
```

A bare `return;` is also a `void` return. Mixing a value-returning statement and a bare `return;` in the same function is a return type mismatch.

### 4.3 Function Parameter Type Inference

Function parameter types are determined by the first call site.

```b
auto func(auto x, auto y) {
    ...
}

A() {
    ...
    func(alpha, beta);
    ...
}
```

Inference rules:

```text
type(x) := inferred_type(alpha)
type(y) := inferred_type(beta)
```

After the parameter types are fixed, every later call to the same function must pass arguments with the same types and the same number of parameters.

## 5. Required Symbol Table Behavior

The translator must build and update symbol tables while traversing the parse tree.

### STEP 1 - Build Symbol Tables

`SymbolTableVisitor` builds symbol tables for global variables and functions.

```cpp
class SymbolTableVisitor : public BBaseVisitor {
    // build symbol tables for functions and globals
};
```

### STEP 2 - Infer Types

`TypeAnalysisVisitor` infers all `auto` types and updates the symbol table.

```cpp
class TypeAnalysisVisitor : public BBaseVisitor {
    // infer types for auto variables and functions
};
```

### STEP 3 - Print Typed Code

`PrintTreeVisitor` must print C/C++ code with the correct inferred types instead of `auto`.

```cpp
class PrintTreeVisitor : public BBaseVisitor {
    // print typed C/C++ code
};
```

The main flow is:

```cpp
int main(int argc, const char* argv[]) {
    // ANTLR parsing
    ParseTree* tree = parser.program();

    // STEP 1. build symbol table
    SymbolTableVisitor SymtabTree;
    SymtabTree.visit(tree);

    // STEP 2. infer types for auto vars/fns
    TypeAnalysisVisitor AnalyzeTree;
    AnalyzeTree.visit(tree);

    // STEP 3. print out correctly typed C code
    PrintTreeVisitor PrintTree;
    PrintTree.visit(tree);

    return 0;
}
```

## 6. Type Error Cases

The translator must detect the following type errors. On the first type error, it must print an error message and exit.

### 6.1 Different Types for the Same Variable

```b
auto x;
...
x = alpha;
...
x = beta;
```

If `x` was first inferred from `alpha`, a later assignment from `beta` is valid only when both inferred types are the same.

```text
type(x) := inferred_type(alpha)
type(x) != inferred_type(beta) => type mismatch error
```

### 6.2 Different Parameter Types for the Same Function

```b
auto func(auto x, auto y) {
    ...
}

A() {
    func(alpha1, beta1);
    ...
    func(alpha2, beta2);
}
```

Errors occur when later call sites do not match the parameter types inferred from the first call site.

```text
inferred_type(alpha1) != inferred_type(alpha2) => parameter type error
inferred_type(beta1)  != inferred_type(beta2)  => parameter type error
```

### 6.3 Different Number of Parameters

Every call to the same function must use the same number of parameters as the function declaration and the other call sites.

```b
auto func(auto, auto);

A() {
    func(alpha, beta, gamma);  // error: too many arguments
    func(alpha, beta);         // valid only if declaration has two parameters
    func(alpha);               // error: too few arguments
}
```

Error rule:

```text
num_of_params(func) != num_of_params(func_call_siteN) => parameter count error
```

### 6.4 Function Return Type Mismatch

All return statements in the same function must resolve to the same type.

```b
auto func() {
    if (...) return alpha;
    else if (...) return beta;
    else return;
}
```

Errors occur when return expressions have different types, or when a value-returning statement is mixed with a `void` return.

```text
inferred_type(alpha) != inferred_type(beta) => return type error
inferred_type(func_return) != void when mixed with return; => return type error
```

### 6.5 Function Return Used in an Incompatible Context

A function call expression has the function's inferred return type. Assignment from a function call must match the destination variable type.

```b
A() {
    auto a = 1;      // a: int
    a = func(alpha); // valid only if func(alpha) returns int
}
```

Error rule:

```text
inferred_type(a) != inferred_type(func(alpha)) => type mismatch error
```

### 6.6 Undefined Variables

An `auto` variable whose type is never inferred is an undefined type error.

```b
auto x, y, z;
...
x = alpha;
...
z = beta;
```

Inference results:

```text
type(x) := inferred_type(alpha)
type(y) := UNDEFINED => error
type(z) := inferred_type(beta)
```

### 6.7 Undefined Functions

A function type is undefined if the translator cannot infer its return type or parameter types.

```b
auto func(auto x, auto y);
// no definition of func(...)

A() {
    // no call site of func(alpha, beta)
}
```

Errors:

```text
inferred_type(func_return) == UNDEFINED => error
inferred_type(x) == UNDEFINED => error
inferred_type(y) == UNDEFINED => error
```

## 7. `#define` and `#include` Handling

The translator does not need to analyze macros or header files.

Rules:

- Do not look into header files.
- Do not analyze macro definitions.
- Do not register macro functions in the symbol table.
- Treat macro functions as externally defined functions.
- Assume macro functions return the appropriate type required by the context.

Example:

```c
#include <stdio.h>
#include "myheader.h"

#define fn(x) arr[x]
```

B code:

```b
auto x = 1;
auto y = x + fn(x);
...
auto f = 1.0;
auto g = f + fn(f);
```

Expected inference:

```text
x: int
fn(x): int in the expression x + fn(x)
y: int

f: double
fn(f): double in the expression f + fn(f)
g: double
```

The same macro-like function can therefore be treated as returning different appropriate types in different contexts, because it is not registered or checked as a normal B function.

## 8. Build, Run, and Validation

The program must accept an input file from the command line and print translated code to `stdout`.

```bash
./b2c input.b | indent > output.c
```

The generated output must compile without type-related warnings or errors.

```bash
g++ -Wconversion -Wall -pedantic output.c
```

The provided `Makefile` may run a similar sequence:

```bash
make run
./b2c input.b | indent > output.c
g++ -Wconversion -Wall -pedantic output.c
```

## 9. Files Provided in the Skeleton

The skeleton code includes:

```text
B2CMain.cpp
B.g4
Makefile
INPUTS/input*.b
```

Required work:

- Complete STEP 2 in `B2CMain.cpp`.
- Fix STEP 3 in `B2CMain.cpp`.
- Use the provided `B.g4` grammar.
- Use the visitor-based structure already provided by the skeleton.

## 10. Submission Policy

Submit only:

```text
B2CMain.cpp
```

The grader will use the other files from `B2C.zip`.

If modifying files other than `B2CMain.cpp` is necessary, ask through the Q&A board before submitting.

## 11. Academic Integrity Policy

Discussion is allowed, but plagiarism is not allowed.

If any code is copied from another person or from the internet, the submission receives 0 points. The policy applies to this project and all following projects.

## 12. Implementation Checklist

Use this checklist before submission.

- [ ] Global variables are registered in the symbol table.
- [ ] Function declarations and definitions are registered in the symbol table.
- [ ] Variable types are inferred from first assignment or initializer.
- [ ] Function return types are inferred from return statements.
- [ ] Functions with no return statement are inferred as `void`.
- [ ] Function parameter types are inferred from the first call site.
- [ ] Later assignments are checked against the already inferred variable type.
- [ ] Later function calls are checked against the already inferred parameter types.
- [ ] Function call argument counts are checked.
- [ ] Return statements in the same function are checked for consistency.
- [ ] Binary and other typed operations reject operands with different types.
- [ ] Undefined variable types are reported.
- [ ] Undefined function return or parameter types are reported.
- [ ] Macros are not registered as normal functions.
- [ ] Header files are not analyzed.
- [ ] `auto` is replaced by concrete types in printed output.
- [ ] The program exits on the first type error.
- [ ] Error output includes the source line number and error type.
- [ ] Generated code compiles with `g++ -Wconversion -Wall -pedantic`.

## 13. Source PDF Coverage Checklist

| PDF page | Covered content |
| --- | --- |
| Page 1 | Title slide: Type Inference, PA #2 - B2C translator |
| Page 2 | Assignment overview, supported types, ANTLR visitor-based B-to-C translator, symbol table update, inference targets, error reporting |
| Page 3 | Strong typing rule and inference rules for variables, function returns, and function parameters |
| Page 4 | Inconsistent type errors for variables and function parameters |
| Page 5 | Function parameter count errors, return type errors, no-return-as-void rule, function return assignment mismatch |
| Page 6 | Undefined variable and undefined function type errors |
| Page 7 | `#define` and `#include` handling, macro functions as externally defined/context-typed functions |
| Page 8 | Skeleton files, visitor classes, and main STEP 1/2/3 traversal flow |
| Page 9 | PA #2 requirements, stdout behavior, compile command, first-error exit rule |
| Page 10 | Submission and grading policy, submit only `B2CMain.cpp`, plagiarism warning |

## 14. Original PDF Slide Captures

The following rendered slide images are included so the README preserves the visual information from the PDF as well as the extracted text.

<details>
<summary>Page 1 - Title</summary>

![Page 1 - Type Inference title slide](readme_assets/pa2-b2c-type-inference/page-01.png)

</details>

<details>
<summary>Page 2 - Programming Assignment #2</summary>

![Page 2 - Programming Assignment overview](readme_assets/pa2-b2c-type-inference/page-02.png)

</details>

<details>
<summary>Page 3 - Type Inference</summary>

![Page 3 - Type inference rules](readme_assets/pa2-b2c-type-inference/page-03.png)

</details>

<details>
<summary>Page 4 - Inconsistent Type Errors</summary>

![Page 4 - Inconsistent variable and parameter types](readme_assets/pa2-b2c-type-inference/page-04.png)

</details>

<details>
<summary>Page 5 - Inconsistent Type Errors Continued</summary>

![Page 5 - Function parameter count and return type errors](readme_assets/pa2-b2c-type-inference/page-05.png)

</details>

<details>
<summary>Page 6 - Undefined Types</summary>

![Page 6 - Undefined variables and functions](readme_assets/pa2-b2c-type-inference/page-06.png)

</details>

<details>
<summary>Page 7 - define and include</summary>

![Page 7 - define and include handling](readme_assets/pa2-b2c-type-inference/page-07.png)

</details>

<details>
<summary>Page 8 - Skeleton Code</summary>

![Page 8 - Skeleton code and visitor classes](readme_assets/pa2-b2c-type-inference/page-08.png)

</details>

<details>
<summary>Page 9 - PA #2 B2C Translator</summary>

![Page 9 - PA2 translator requirements and commands](readme_assets/pa2-b2c-type-inference/page-09.png)

</details>

<details>
<summary>Page 10 - Submission and Grading Policy</summary>

![Page 10 - Submission and grading policy](readme_assets/pa2-b2c-type-inference/page-10.png)

</details>
