// B2CMain.cpp
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "antlr4-runtime.h"
#include "antlr4-cpp/BBaseVisitor.h"
#include "antlr4-cpp/BLexer.h"
#include "antlr4-cpp/BParser.h"

using namespace std;
using namespace antlr4;
using namespace antlr4::tree;

enum Types {tyAUTO, tyINT, tyDOUBLE, tySTRING, tyBOOL, tyCHAR, tyVOID, tyFUNCTION};
string mnemonicTypes[] = {"auto", "int", "double", "string", "bool", "char", "void", "function"};

struct SymbolAttributes {
    Types type; // int, double, bool, char, string, function --- auto if unknown yet
    bool defined = false;
    int line = 0;

    // if type == function, first element is the return type and the rest are argument types
    vector<Types> retArgTypes;
};

static void print_error_and_exit(int line, const string& message) {
    cerr << "[ERROR] line " << line << ": " << message << endl;
    exit(-1);
}

class SymbolTable {
private:
    map<string, SymbolAttributes> table;

public:
    void addSymbol(const string& name, const SymbolAttributes& attributes) {
        table[name] = attributes;
    }

    bool symbolExists(const string& name) const {
        return table.find(name) != table.end();
    }

    SymbolAttributes getSymbolAttributes(const string& name) const {
        auto it = table.find(name);
        if (it != table.end()) {
            return it->second;
        }
        cerr << "Error: Symbol " << name << " not found" << endl;
        exit(-1);
    }

    void setSymbolAttributes(const string& name, const SymbolAttributes& attributes) {
        table[name] = attributes;
    }

    const map<string, SymbolAttributes>& symbols() const {
        return table;
    }

    void printSymbols() const {
        for (const auto& pair : table) {
            cout << "(name) " << pair.first << ", (type) " << mnemonicTypes[pair.second.type];
            if (pair.second.type == tyFUNCTION) {
                cout << "| ";
                int n = static_cast<int>(pair.second.retArgTypes.size());
                if (n > 0) {
                    cout << mnemonicTypes[pair.second.retArgTypes[0]] << "(";
                }
                for (int i = 1; i < n - 1; i++) {
                    cout << mnemonicTypes[pair.second.retArgTypes[i]] << ", ";
                }
                if (n > 1) {
                    cout << mnemonicTypes[pair.second.retArgTypes[n - 1]];
                }
                cout << ")";
            }
            cout << endl;
        }
    }
};

/*
 * STEP 1. build symbol table
 */
const string _GlobalFuncName_ = "$_global_$";

map<string, SymbolTable*> symTabs;
map<string, int> blockCounters;
map<string, string> scopeParents;
map<string, vector<string>> functionParams;

static Types constantType(BParser::ConstantContext *ctx) {
    if (ctx->INT()) return tyINT;
    if (ctx->REAL()) return tyDOUBLE;
    if (ctx->STRING()) return tySTRING;
    if (ctx->BOOL()) return tyBOOL;
    if (ctx->CHAR()) return tyCHAR;
    print_error_and_exit(ctx->getStart()->getLine(), "unrecognizable constant");
    return tyAUTO;
}

static bool autostmtHasInitializer(BParser::AutostmtContext *ctx, int nameIndex) {
    for (size_t i = 0; i < ctx->children.size(); i++) {
        if (ctx->children[i] == ctx->name(nameIndex)) {
            return i + 1 < ctx->children.size() && ctx->children[i + 1]->getText() == "=";
        }
    }
    return false;
}

static int initializerIndexForName(BParser::AutostmtContext *ctx, int nameIndex) {
    int index = 0;
    for (int i = 0; i < nameIndex; i++) {
        if (autostmtHasInitializer(ctx, i)) {
            index++;
        }
    }
    return index;
}

static string nextBlockScope(const string& parentScope) {
    int next = ++blockCounters[parentScope];
    return parentScope + "$" + to_string(next);
}

static SymbolTable* ensureScope(const string& scope) {
    if (symTabs.find(scope) == symTabs.end()) {
        symTabs[scope] = new SymbolTable();
    }
    return symTabs[scope];
}

static bool lookupSymbol(const string& startScope, const string& name, string& foundScope, SymbolAttributes& attr) {
    string scope = startScope;
    while (!scope.empty()) {
        auto tableIt = symTabs.find(scope);
        if (tableIt != symTabs.end() && tableIt->second->symbolExists(name)) {
            foundScope = scope;
            attr = tableIt->second->getSymbolAttributes(name);
            return true;
        }
        auto parentIt = scopeParents.find(scope);
        if (parentIt == scopeParents.end()) {
            break;
        }
        scope = parentIt->second;
    }
    return false;
}

static bool getGlobalFunction(const string& name, SymbolAttributes& attr) {
    auto it = symTabs.find(_GlobalFuncName_);
    if (it == symTabs.end() || !it->second->symbolExists(name)) {
        return false;
    }
    attr = it->second->getSymbolAttributes(name);
    return attr.type == tyFUNCTION;
}

static void setGlobalFunction(const string& name, const SymbolAttributes& attr) {
    ensureScope(_GlobalFuncName_)->setSymbolAttributes(name, attr);
}

class SymbolTableVisitor : public BBaseVisitor {
private:
    string curScope;

    void addFunctionSymbol(const string& funcName, const vector<Types>& funcTypes,
                           bool isDefinition, int line) {
        SymbolTable *global = ensureScope(_GlobalFuncName_);

        if (global->symbolExists(funcName)) {
            SymbolAttributes existingAttr = global->getSymbolAttributes(funcName);
            if (existingAttr.type != tyFUNCTION) {
                print_error_and_exit(line, "duplicate symbol declaration: " + funcName);
            }
            if (existingAttr.retArgTypes.size() != funcTypes.size()) {
                print_error_and_exit(line, "conflicting function declaration: " + funcName);
            }
            if (isDefinition && existingAttr.defined) {
                print_error_and_exit(line, "duplicate function definition: " + funcName);
            }
            existingAttr.defined = existingAttr.defined || isDefinition;
            global->setSymbolAttributes(funcName, existingAttr);
            return;
        }

        SymbolAttributes funcAttr;
        funcAttr.type = tyFUNCTION;
        funcAttr.defined = isDefinition;
        funcAttr.line = line;
        funcAttr.retArgTypes = funcTypes;
        global->addSymbol(funcName, funcAttr);
    }

public:
    any visitProgram(BParser::ProgramContext *ctx) override {
        symTabs.clear();
        blockCounters.clear();
        scopeParents.clear();
        functionParams.clear();

        curScope = _GlobalFuncName_;
        ensureScope(curScope);
        scopeParents[curScope] = "";

        for (auto child : ctx->children) {
            visit(child);
        }
        return nullptr;
    }

    any visitDefinition(BParser::DefinitionContext *ctx) override {
        visit(ctx->children[0]);
        return nullptr;
    }

    any visitAutostmt(BParser::AutostmtContext *ctx) override {
        SymbolTable *stab = ensureScope(curScope);

        for (int i = 0; i < static_cast<int>(ctx->name().size()); i++) {
            string varName = ctx->name(i)->getText();
            if (stab->symbolExists(varName)) {
                print_error_and_exit(ctx->name(i)->getStart()->getLine(),
                                     "duplicate variable declaration: " + varName);
            }

            Types varType = tyAUTO;
            if (autostmtHasInitializer(ctx, i)) {
                varType = constantType(ctx->constant(initializerIndexForName(ctx, i)));
            }
            SymbolAttributes varAttr;
            varAttr.type = varType;
            varAttr.defined = true;
            varAttr.line = ctx->name(i)->getStart()->getLine();
            stab->addSymbol(varName, varAttr);
        }
        return nullptr;
    }

    any visitDeclstmt(BParser::DeclstmtContext *ctx) override {
        if (curScope != _GlobalFuncName_) {
            print_error_and_exit(ctx->getStart()->getLine(),
                                 "function declaration is only allowed in global scope");
        }

        vector<Types> funcTypes(ctx->AUTO().size(), tyAUTO);
        addFunctionSymbol(ctx->name()->getText(), funcTypes, false, ctx->name()->getStart()->getLine());
        return nullptr;
    }

    any visitFuncdef(BParser::FuncdefContext *ctx) override {
        if (curScope != _GlobalFuncName_) {
            print_error_and_exit(ctx->getStart()->getLine(),
                                 "function definition is only allowed in global scope");
        }

        string funcName = ctx->name(0)->getText();
        vector<Types> funcTypes(ctx->AUTO().size(), tyAUTO);
        addFunctionSymbol(funcName, funcTypes, true, ctx->name(0)->getStart()->getLine());

        string prevScope = curScope;
        curScope = funcName;
        ensureScope(curScope);
        scopeParents[curScope] = _GlobalFuncName_;

        vector<string> params;
        SymbolTable *funcSymTab = ensureScope(curScope);
        for (int i = 1; i < static_cast<int>(ctx->name().size()); i++) {
            string paramName = ctx->name(i)->getText();
            if (funcSymTab->symbolExists(paramName)) {
                print_error_and_exit(ctx->name(i)->getStart()->getLine(),
                                     "duplicate parameter name: " + paramName);
            }
            params.push_back(paramName);
            SymbolAttributes paramAttr;
            paramAttr.type = tyAUTO;
            paramAttr.defined = true;
            paramAttr.line = ctx->name(i)->getStart()->getLine();
            funcSymTab->addSymbol(paramName, paramAttr);
        }
        functionParams[funcName] = params;

        for (auto stmt : ctx->blockstmt()->statement()) {
            visit(stmt);
        }

        curScope = prevScope;
        return nullptr;
    }

    any visitBlockstmt(BParser::BlockstmtContext *ctx) override {
        string blockScope = nextBlockScope(curScope);
        ensureScope(blockScope);
        scopeParents[blockScope] = curScope;

        string prevScope = curScope;
        curScope = blockScope;
        for (auto stmt : ctx->statement()) {
            visit(stmt);
        }
        curScope = prevScope;

        return nullptr;
    }

    any visitStatement(BParser::StatementContext *ctx) override {
        visit(ctx->children[0]);
        return nullptr;
    }
};

/*
 * STEP 2. infer type
 */
class TypeAnalysisVisitor : public BBaseVisitor {
private:
    string curScope;
    string curFuncName;
    bool changed = false;
    bool sawReturn = false;

    bool isKnown(Types type) const {
        return type != tyAUTO;
    }

    bool isValueType(Types type) const {
        return type != tyAUTO && type != tyVOID && type != tyFUNCTION;
    }

    void requireValueType(Types type, int line, const string& context) {
        if (type == tyVOID || type == tyFUNCTION) {
            print_error_and_exit(line, "invalid " + context + " type: " + mnemonicTypes[type]);
        }
    }

    void assignVariableType(const string& name, Types rhsType, int line) {
        if (!isKnown(rhsType)) {
            return;
        }
        if (rhsType == tyVOID || rhsType == tyFUNCTION) {
            print_error_and_exit(line, "invalid assignment from " + mnemonicTypes[rhsType]);
        }

        string foundScope;
        SymbolAttributes attr;
        if (!lookupSymbol(curScope, name, foundScope, attr) || attr.type == tyFUNCTION) {
            print_error_and_exit(line, "undefined variable: " + name);
        }

        if (attr.type == tyAUTO) {
            attr.type = rhsType;
            symTabs[foundScope]->setSymbolAttributes(name, attr);
            changed = true;
        } else if (attr.type != rhsType) {
            print_error_and_exit(line, "type mismatch for variable " + name);
        }
    }

    Types readVariableType(const string& name, int line) {
        string foundScope;
        SymbolAttributes attr;
        if (!lookupSymbol(curScope, name, foundScope, attr) || attr.type == tyFUNCTION) {
            print_error_and_exit(line, "undefined variable: " + name);
        }
        return attr.type;
    }

    void updateFunctionParam(const string& funcName, int paramIndex, Types argType, int line) {
        if (!isKnown(argType)) {
            return;
        }
        requireValueType(argType, line, "parameter");

        SymbolAttributes funcAttr;
        if (!getGlobalFunction(funcName, funcAttr)) {
            return;
        }

        Types currentType = funcAttr.retArgTypes[paramIndex + 1];
        if (currentType == tyAUTO) {
            funcAttr.retArgTypes[paramIndex + 1] = argType;
            setGlobalFunction(funcName, funcAttr);
            changed = true;

            auto paramsIt = functionParams.find(funcName);
            if (paramsIt != functionParams.end() && paramIndex < static_cast<int>(paramsIt->second.size())) {
                SymbolTable *funcTable = ensureScope(funcName);
                string paramName = paramsIt->second[paramIndex];
                if (funcTable->symbolExists(paramName)) {
                    SymbolAttributes paramAttr = funcTable->getSymbolAttributes(paramName);
                    paramAttr.type = argType;
                    funcTable->setSymbolAttributes(paramName, paramAttr);
                }
            }
        } else if (currentType != argType) {
            print_error_and_exit(line, "parameter type mismatch in call to " + funcName);
        }
    }

    void updateFunctionReturn(const string& funcName, Types returnType, int line) {
        if (!isKnown(returnType)) {
            return;
        }
        if (returnType == tyFUNCTION) {
            print_error_and_exit(line, "invalid function return type");
        }

        SymbolAttributes funcAttr;
        if (!getGlobalFunction(funcName, funcAttr)) {
            return;
        }

        Types currentType = funcAttr.retArgTypes[0];
        if (currentType == tyAUTO) {
            funcAttr.retArgTypes[0] = returnType;
            setGlobalFunction(funcName, funcAttr);
            changed = true;
        } else if (currentType != returnType) {
            print_error_and_exit(line, "return type mismatch in function " + funcName);
        }
    }

    Types inferFuncInvocation(BParser::FuncinvocationContext *ctx, Types expectedType) {
        string funcName = ctx->name()->getText();
        int line = ctx->name()->getStart()->getLine();

        SymbolAttributes funcAttr;
        if (!getGlobalFunction(funcName, funcAttr)) {
            for (auto expr : ctx->expr()) {
                inferExpr(expr, tyAUTO);
            }
            return expectedType;
        }

        int expectedArgs = static_cast<int>(funcAttr.retArgTypes.size()) - 1;
        int actualArgs = static_cast<int>(ctx->expr().size());
        if (expectedArgs != actualArgs) {
            print_error_and_exit(line, "parameter count mismatch in call to " + funcName);
        }

        for (int i = 0; i < actualArgs; i++) {
            Types paramType = funcAttr.retArgTypes[i + 1];
            Types argType = inferExpr(ctx->expr(i), paramType);

            if (paramType == tyAUTO && isKnown(argType)) {
                updateFunctionParam(funcName, i, argType, line);
                getGlobalFunction(funcName, funcAttr);
                paramType = funcAttr.retArgTypes[i + 1];
            }

            if (isKnown(paramType) && isKnown(argType) && paramType != argType) {
                print_error_and_exit(line, "parameter type mismatch in call to " + funcName);
            }
        }

        return funcAttr.retArgTypes[0];
    }

    Types inferAtom(BParser::AtomContext *ctx, Types expectedType) {
        if (ctx->constant()) {
            return constantType(ctx->constant());
        }
        if (ctx->name()) {
            return readVariableType(ctx->name()->getText(), ctx->name()->getStart()->getLine());
        }
        if (ctx->expression()) {
            return inferExpression(ctx->expression(), expectedType);
        }
        if (ctx->funcinvocation()) {
            return inferFuncInvocation(ctx->funcinvocation(), expectedType);
        }
        return tyAUTO;
    }

    Types inferExpr(BParser::ExprContext *ctx, Types expectedType = tyAUTO) {
        int line = ctx->getStart()->getLine();

        if (ctx->atom()) {
            Types type = inferAtom(ctx->atom(), expectedType);
            if (ctx->NOT()) {
                if (isKnown(type) && type != tyBOOL) {
                    print_error_and_exit(line, "operator ! requires bool");
                }
                return tyBOOL;
            }
            if (ctx->PLUS() || ctx->MINUS()) {
                if (isKnown(type) && (type == tyBOOL || type == tySTRING || type == tyVOID || type == tyFUNCTION)) {
                    print_error_and_exit(line, "invalid unary operand type");
                }
                return type;
            }
            return type;
        }

        if (ctx->AND() || ctx->OR()) {
            Types left = inferExpr(ctx->expr(0), tyBOOL);
            Types right = inferExpr(ctx->expr(1), tyBOOL);
            if ((isKnown(left) && left != tyBOOL) || (isKnown(right) && right != tyBOOL)) {
                print_error_and_exit(line, "logical operators require bool operands");
            }
            return tyBOOL;
        }

        if (ctx->QUEST()) {
            Types cond = inferExpr(ctx->expr(0), tyBOOL);
            if (isKnown(cond) && cond != tyBOOL) {
                print_error_and_exit(line, "ternary condition must be bool");
            }

            Types trueType = inferExpr(ctx->expr(1), expectedType);
            Types falseExpected = isKnown(trueType) ? trueType : expectedType;
            Types falseType = inferExpr(ctx->expr(2), falseExpected);
            if (!isKnown(trueType) && isKnown(falseType)) {
                trueType = inferExpr(ctx->expr(1), falseType);
            }
            if (isKnown(trueType)) {
                requireValueType(trueType, line, "ternary branch");
            }
            if (isKnown(falseType)) {
                requireValueType(falseType, line, "ternary branch");
            }
            if (isKnown(trueType) && isKnown(falseType) && trueType != falseType) {
                print_error_and_exit(line, "ternary branch type mismatch");
            }
            return isKnown(trueType) ? trueType : falseType;
        }

        Types left = inferExpr(ctx->expr(0), tyAUTO);
        Types rightExpected = isKnown(left) ? left : tyAUTO;
        Types right = inferExpr(ctx->expr(1), rightExpected);
        if (!isKnown(left) && isKnown(right)) {
            left = inferExpr(ctx->expr(0), right);
        }

        if (isKnown(left) && isKnown(right) && left != right) {
            print_error_and_exit(line, "binary operand type mismatch");
        }
        if (isKnown(left)) {
            requireValueType(left, line, "binary operand");
        }
        if (isKnown(right)) {
            requireValueType(right, line, "binary operand");
        }

        if (ctx->GT() || ctx->GTE() || ctx->LT() || ctx->LTE() || ctx->EQ() || ctx->NEQ()) {
            return tyBOOL;
        }

        return isKnown(left) ? left : right;
    }

    Types inferExpression(BParser::ExpressionContext *ctx, Types expectedType = tyAUTO) {
        int line = ctx->getStart()->getLine();
        if (ctx->ASSN()) {
            string lhsName = ctx->name()->getText();
            Types lhsType = readVariableType(lhsName, ctx->name()->getStart()->getLine());
            Types rhsType = inferExpr(ctx->expr(), lhsType);
            if (!isKnown(rhsType) && isKnown(expectedType)) {
                rhsType = inferExpr(ctx->expr(), expectedType);
            }
            assignVariableType(lhsName, rhsType, line);
            return isKnown(lhsType) ? lhsType : rhsType;
        }
        return inferExpr(ctx->expr(), expectedType);
    }

    void validateUndefinedTypes() {
        for (const auto& scopePair : symTabs) {
            for (const auto& symbolPair : scopePair.second->symbols()) {
                const string& name = symbolPair.first;
                const SymbolAttributes& attr = symbolPair.second;

                if (attr.type == tyAUTO) {
                    print_error_and_exit(attr.line, "undefined type for variable " + name);
                }

                if (attr.type == tyFUNCTION) {
                    if (!attr.defined) {
                        print_error_and_exit(attr.line, "undefined function: " + name);
                    }
                    for (int i = 0; i < static_cast<int>(attr.retArgTypes.size()); i++) {
                        if (attr.retArgTypes[i] == tyAUTO) {
                            print_error_and_exit(attr.line, "undefined type for function " + name);
                        }
                    }
                }
            }
        }
    }

public:
    bool didChange() const {
        return changed;
    }

    void finalize() {
        validateUndefinedTypes();
    }

    any visitProgram(BParser::ProgramContext *ctx) override {
        changed = false;
        blockCounters.clear();
        curScope = _GlobalFuncName_;
        curFuncName = "";

        for (auto child : ctx->children) {
            visit(child);
        }
        return nullptr;
    }

    any visitDefinition(BParser::DefinitionContext *ctx) override {
        visit(ctx->children[0]);
        return nullptr;
    }

    any visitFuncdef(BParser::FuncdefContext *ctx) override {
        string prevScope = curScope;
        string prevFunc = curFuncName;
        bool prevSawReturn = sawReturn;

        curFuncName = ctx->name(0)->getText();
        curScope = curFuncName;
        sawReturn = false;

        for (auto stmt : ctx->blockstmt()->statement()) {
            visit(stmt);
        }

        if (!sawReturn) {
            updateFunctionReturn(curFuncName, tyVOID, ctx->name(0)->getStart()->getLine());
        }

        curScope = prevScope;
        curFuncName = prevFunc;
        sawReturn = prevSawReturn;
        return nullptr;
    }

    any visitBlockstmt(BParser::BlockstmtContext *ctx) override {
        string blockScope = nextBlockScope(curScope);
        string prevScope = curScope;
        curScope = blockScope;

        for (auto stmt : ctx->statement()) {
            visit(stmt);
        }

        curScope = prevScope;
        return nullptr;
    }

    any visitStatement(BParser::StatementContext *ctx) override {
        visit(ctx->children[0]);
        return nullptr;
    }

    any visitExpressionstmt(BParser::ExpressionstmtContext *ctx) override {
        inferExpression(ctx->expression(), tyAUTO);
        return nullptr;
    }

    any visitReturnstmt(BParser::ReturnstmtContext *ctx) override {
        sawReturn = true;
        Types returnType = tyVOID;
        if (ctx->expression()) {
            returnType = inferExpression(ctx->expression(), tyAUTO);
        }
        updateFunctionReturn(curFuncName, returnType, ctx->getStart()->getLine());
        return nullptr;
    }

    any visitIfstmt(BParser::IfstmtContext *ctx) override {
        Types condType = inferExpr(ctx->expr(), tyBOOL);
        if (isKnown(condType) && condType != tyBOOL) {
            print_error_and_exit(ctx->getStart()->getLine(), "if condition must be bool");
        }
        visit(ctx->statement(0));
        if (ctx->ELSE()) {
            visit(ctx->statement(1));
        }
        return nullptr;
    }

    any visitWhilestmt(BParser::WhilestmtContext *ctx) override {
        Types condType = inferExpr(ctx->expr(), tyBOOL);
        if (isKnown(condType) && condType != tyBOOL) {
            print_error_and_exit(ctx->getStart()->getLine(), "while condition must be bool");
        }
        visit(ctx->statement());
        return nullptr;
    }
};

/*
 * STEP 3. print code
 */
class PrintTreeVisitor : public BBaseVisitor {
private:
    string curScope;
    string curFunctionName;

    string typeName(Types type) {
        return mnemonicTypes[type];
    }

    string functionReturnType(const string& functionName, Types type) {
        if (functionName == "main" && type == tyVOID) {
            return "int";
        }
        return typeName(type);
    }

    SymbolAttributes symbolInCurrentScope(const string& name, int line) {
        auto it = symTabs.find(curScope);
        if (it == symTabs.end() || !it->second->symbolExists(name)) {
            print_error_and_exit(line, "internal error: missing symbol " + name);
        }
        return it->second->getSymbolAttributes(name);
    }

    Types visibleSymbolType(const string& name, int line) {
        string foundScope;
        SymbolAttributes attr;
        if (!lookupSymbol(curScope, name, foundScope, attr)) {
            print_error_and_exit(line, "internal error: missing symbol " + name);
        }
        return attr.type;
    }

public:
    any visitProgram(BParser::ProgramContext *ctx) override {
        blockCounters.clear();
        curScope = _GlobalFuncName_;
        curFunctionName = "";
        for (auto child : ctx->children) {
            visit(child);
        }
        return nullptr;
    }

    any visitDirective(BParser::DirectiveContext *ctx) override {
        cout << ctx->SHARP_DIRECTIVE()->getText() << endl;
        return nullptr;
    }

    any visitDefinition(BParser::DefinitionContext *ctx) override {
        visit(ctx->children[0]);
        return nullptr;
    }

    any visitFuncdef(BParser::FuncdefContext *ctx) override {
        string functionName = ctx->name(0)->getText();
        SymbolAttributes funcAttr;
        getGlobalFunction(functionName, funcAttr);

        cout << functionReturnType(functionName, funcAttr.retArgTypes[0]) << " " << functionName << "(";
        for (int i = 1; i < static_cast<int>(ctx->name().size()); i++) {
            if (i != 1) cout << ", ";
            cout << typeName(funcAttr.retArgTypes[i]) << " " << ctx->name(i)->getText();
        }
        cout << ")";

        string prevScope = curScope;
        string prevFunctionName = curFunctionName;
        curScope = functionName;
        curFunctionName = functionName;
        cout << " ";
        cout << "{" << endl;
        for (auto stmt : ctx->blockstmt()->statement()) {
            visit(stmt);
        }
        cout << "}" << endl;
        curScope = prevScope;
        curFunctionName = prevFunctionName;
        return nullptr;
    }

    any visitStatement(BParser::StatementContext *ctx) override {
        visit(ctx->children[0]);
        return nullptr;
    }

    any visitAutostmt(BParser::AutostmtContext *ctx) override {
        for (int i = 0; i < static_cast<int>(ctx->name().size()); i++) {
            string varName = ctx->name(i)->getText();
            SymbolAttributes attr = symbolInCurrentScope(varName, ctx->name(i)->getStart()->getLine());

            cout << typeName(attr.type) << " " << varName;
            if (autostmtHasInitializer(ctx, i)) {
                cout << " = ";
                visit(ctx->constant(initializerIndexForName(ctx, i)));
            }
            cout << ";" << endl;
        }
        return nullptr;
    }

    any visitDeclstmt(BParser::DeclstmtContext *ctx) override {
        string functionName = ctx->name()->getText();
        SymbolAttributes funcAttr;
        getGlobalFunction(functionName, funcAttr);

        cout << typeName(funcAttr.retArgTypes[0]) << " " << functionName << "(";
        for (int i = 1; i < static_cast<int>(funcAttr.retArgTypes.size()); i++) {
            if (i != 1) cout << ", ";
            cout << typeName(funcAttr.retArgTypes[i]);
        }
        cout << ");" << endl;
        return nullptr;
    }

    any visitBlockstmt(BParser::BlockstmtContext *ctx) override {
        string blockScope = nextBlockScope(curScope);
        string prevScope = curScope;
        curScope = blockScope;

        cout << "{" << endl;
        for (auto stmt : ctx->statement()) {
            visit(stmt);
        }
        cout << "}" << endl;

        curScope = prevScope;
        return nullptr;
    }

    any visitIfstmt(BParser::IfstmtContext *ctx) override {
        cout << "if (";
        visit(ctx->expr());
        cout << ") ";
        visit(ctx->statement(0));
        if (ctx->ELSE()) {
            cout << "else ";
            visit(ctx->statement(1));
        }
        return nullptr;
    }

    any visitWhilestmt(BParser::WhilestmtContext *ctx) override {
        cout << "while (";
        visit(ctx->expr());
        cout << ") ";
        visit(ctx->statement());
        return nullptr;
    }

    any visitExpressionstmt(BParser::ExpressionstmtContext *ctx) override {
        visit(ctx->expression());
        cout << ";" << endl;
        return nullptr;
    }

    any visitReturnstmt(BParser::ReturnstmtContext *ctx) override {
        cout << "return";
        if (ctx->expression()) {
            cout << " ";
            SymbolAttributes funcAttr;
            if (!curFunctionName.empty() && getGlobalFunction(curFunctionName, funcAttr)
                && funcAttr.retArgTypes[0] == tyCHAR) {
                cout << "(char)(";
                visit(ctx->expression());
                cout << ")";
            } else {
                visit(ctx->expression());
            }
        }
        cout << ";" << endl;
        return nullptr;
    }

    any visitNullstmt(BParser::NullstmtContext *ctx) override {
        cout << ";" << endl;
        return nullptr;
    }

    any visitExpr(BParser::ExprContext *ctx) override {
        if (ctx->atom()) {
            if (ctx->PLUS()) cout << "+";
            else if (ctx->MINUS()) cout << "-";
            else if (ctx->NOT()) cout << "!";
            visit(ctx->atom());
        } else if (ctx->MUL() || ctx->DIV() || ctx->PLUS() || ctx->MINUS() ||
                   ctx->GT() || ctx->GTE() || ctx->LT() || ctx->LTE() ||
                   ctx->EQ() || ctx->NEQ() || ctx->AND() || ctx->OR()) {
            visit(ctx->expr(0));
            cout << " " << ctx->children[1]->getText() << " ";
            visit(ctx->expr(1));
        } else if (ctx->QUEST()) {
            visit(ctx->expr(0));
            cout << " ? ";
            visit(ctx->expr(1));
            cout << " : ";
            visit(ctx->expr(2));
        } else {
            print_error_and_exit(ctx->getStart()->getLine(), "unrecognized expression");
        }
        return nullptr;
    }

    any visitAtom(BParser::AtomContext *ctx) override {
        if (ctx->expression()) {
            cout << "(";
            visit(ctx->expression());
            cout << ")";
        } else {
            visit(ctx->children[0]);
        }
        return nullptr;
    }

    any visitExpression(BParser::ExpressionContext *ctx) override {
        if (ctx->ASSN()) {
            Types lhsType = visibleSymbolType(ctx->name()->getText(), ctx->name()->getStart()->getLine());
            visit(ctx->name());
            cout << " = ";
            if (lhsType == tyCHAR) {
                cout << "(char)(";
                visit(ctx->expr());
                cout << ")";
                return nullptr;
            }
        }
        visit(ctx->expr());
        return nullptr;
    }

    any visitFuncinvocation(BParser::FuncinvocationContext *ctx) override {
        cout << ctx->name()->getText() << "(";
        for (int i = 0; i < static_cast<int>(ctx->expr().size()); i++) {
            if (i != 0) cout << ", ";
            visit(ctx->expr(i));
        }
        cout << ")";
        return nullptr;
    }

    any visitConstant(BParser::ConstantContext *ctx) override {
        cout << ctx->children[0]->getText();
        return nullptr;
    }

    any visitName(BParser::NameContext *ctx) override {
        cout << ctx->NAME()->getText();
        return nullptr;
    }
};

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        cerr << "[Usage] " << argv[0] << "  <input-file>\n";
        exit(0);
    }

    ifstream stream;
    stream.open(argv[1]);
    if (stream.fail()) {
        cerr << argv[1] << " : file open fail\n";
        exit(0);
    }

    ANTLRInputStream inputStream(stream);
    BLexer lexer(&inputStream);
    CommonTokenStream tokenStream(&lexer);
    BParser parser(&tokenStream);
    ParseTree* tree = parser.program();

    if (parser.getNumberOfSyntaxErrors() > 0) {
        exit(-1);
    }

    SymbolTableVisitor SymtabTree;
    SymtabTree.visit(tree);

    for (int i = 0; i < 20; i++) {
        TypeAnalysisVisitor AnalyzeTree;
        AnalyzeTree.visit(tree);
        if (!AnalyzeTree.didChange()) {
            break;
        }
    }

    TypeAnalysisVisitor FinalCheck;
    FinalCheck.finalize();

    PrintTreeVisitor PrintTree;
    PrintTree.visit(tree);

    return 0;
}
