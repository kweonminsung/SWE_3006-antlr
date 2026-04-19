// B2CMain.cpp
#include <iostream>
#include <map>
#include <stack>
#include "antlr4-runtime.h"
#include "antlr4-cpp/BBaseVisitor.h"
#include "antlr4-cpp/BLexer.h"
#include "antlr4-cpp/BParser.h"

using namespace std;
using namespace antlr4;
using namespace antlr4::tree;

enum Types {tyAUTO, tyINT, tyDOUBLE, tySTRING, tyBOOL, tyCHAR, tyFUNCTION};
string mnemonicTypes[] = {"auto", "int", "double", "string", "bool", "char", "function"};

struct SymbolAttributes {
   Types type; // int, double, bool, char, string, function --- auto if unknown yet
   bool defined = false;

   // if type == "function"
   vector<Types> retArgTypes; // first element is a return_type
};

class SymbolTable {
private:
    map<string, SymbolAttributes> table;  // symbol-name: string, symbol-typeInfo: SymbolAttributes

public:
    // Add a new symbol 
    void addSymbol(const string& name, const SymbolAttributes& attributes) {
		table[name] = attributes; 
    }

    // Check if a symbol exists
    bool symbolExists(const string& name) const {
        return table.find(name) != table.end();
    }

    // Get attributes of a symbol
    SymbolAttributes getSymbolAttributes(const string& name) const {
        if (symbolExists(name)) {
            return table.at(name);
        } else {
            cerr << "Error: Symbol " << name << " not found" << endl;
			exit(-1);
        }
    }

    // Remove a symbol from the table
    void removeSymbol(const string& name) {
        table.erase(name);
    }

    // Print all symbols in the table (for debugging purposes)
    void printSymbols() const {
        for (const auto& pair : table) {
            cout << "(name) " << pair.first << ", (type) " << mnemonicTypes[pair.second.type];
			if (pair.second.type == tyFUNCTION) {
				cout << "| ";
				int n = pair.second.retArgTypes.size();
				if (n > 0) {
					cout << mnemonicTypes[pair.second.retArgTypes[0]] << "("; // return type
				}
				for (int i = 1; i < n-1; i++)
					cout << mnemonicTypes[pair.second.retArgTypes[i]] << ", ";
				if (n > 1) {
					cout << mnemonicTypes[pair.second.retArgTypes[n-1]]; // last arg type
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

// collection of per-function symbol tables accessed by function name
// symbol table in global scope can be accessed with special name defined in _GlobalFuncName_
map<string, SymbolTable*> symTabs;

// Maps scope name to the count of blockstmt children for proper numbering
map<string, int> blockCounters;


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
    	for (int i=0; i< ctx->children.size(); i++) {
    		visit(ctx->children[i]);
    	}

		// print all symbol tables
		for (auto& pair : symTabs) {
	    	cout << "--- Symbol Table --- " << pair.first << endl; // function name
	    	pair.second->printSymbols();					   // per-function symbol table
			cout << "";
		}

    	return nullptr;
	}

    any visitDefinition(BParser::DefinitionContext *ctx) override {
		visit(ctx->children[0]);
        return nullptr;
	}

    any visitAutostmt(BParser::AutostmtContext *ctx) override {
    	// get current symbol table
		if (symTabs.find(curFuncName) == symTabs.end()) {
			symTabs[curFuncName] = new SymbolTable();
		}
		SymbolTable *stab = symTabs[curFuncName];

		// You can retrieve the variable names and constants using ctx->name(i) and ctx->constant(i)
		for (int i=0, j=0; i < ctx->name().size(); i++) {
			
			string varName = ctx->name(i)->getText();
			enum Types varType = tyAUTO;				// default type

			// Check for duplicate symbol at same scope level
			if (stab->symbolExists(varName)) {
				cerr << "[ERROR] Duplicate variable declaration: " << varName << endl;
				exit(-1);
			}

			// if initialized, get constant type
			int idx_assn = 1 + i*2 + j*2 + 1;  // auto name (= const)?, name (= const)?, ...
			if (ctx->children[idx_assn]->getText().compare("=") == 0) { 
				if (ctx->constant(j)) {  
					varType = any_cast<Types>( visit(ctx->constant(j)) );   // returns init constant type
					j++;
				}
			}

			stab->addSymbol(varName, {varType, true});
		}
    	return nullptr;
    }

    any visitDeclstmt(BParser::DeclstmtContext *ctx) override {
		// Function declaration - only in global scope
		if (symTabs.find(curFuncName) == symTabs.end()) {
			symTabs[curFuncName] = new SymbolTable();
		}
		SymbolTable *stab = symTabs[curFuncName];

		string funcName = ctx->name()->getText();

		// Get argument types (all AUTO)
		vector<Types> argTypes;
		argTypes.push_back(tyAUTO);  // return type (first element)
		
		// Count AUTO tokens after first one (for arguments)
		for (int i = 1; i < ctx->AUTO().size(); i++) {
			argTypes.push_back(tyAUTO);
		}

		if (stab->symbolExists(funcName)) {
			SymbolAttributes existingAttr = stab->getSymbolAttributes(funcName);
			if (existingAttr.type != tyFUNCTION) {
				cerr << "[ERROR] Duplicate symbol declaration: " << funcName << endl;
				exit(-1);
			}
			if (existingAttr.retArgTypes != argTypes) {
				cerr << "[ERROR] Conflicting function declaration: " << funcName << endl;
				exit(-1);
			}
			return nullptr;
		}

		SymbolAttributes funcAttr;
		funcAttr.type = tyFUNCTION;
		funcAttr.defined = false;
		funcAttr.retArgTypes = argTypes;
		stab->addSymbol(funcName, funcAttr);

		return nullptr;
    }

    any visitFuncdef(BParser::FuncdefContext *ctx) override {
		// Get current scope
		string symTabName = curFuncName;
		if (symTabs.find(symTabName) == symTabs.end()) {
			symTabs[symTabName] = new SymbolTable();
		}
		SymbolTable *stab = symTabs[symTabName];

		string funcName = ctx->name(0)->getText();

		// Get argument types and names
		vector<Types> funcTypes;
		funcTypes.push_back(tyAUTO);  // return type (first element)
		
		// Count AUTO tokens for arguments
		for (int i = 1; i < ctx->AUTO().size(); i++) {
			funcTypes.push_back(tyAUTO);
		}

		if (stab->symbolExists(funcName)) {
			SymbolAttributes existingAttr = stab->getSymbolAttributes(funcName);
			if (existingAttr.type != tyFUNCTION) {
				cerr << "[ERROR] Duplicate symbol declaration: " << funcName << endl;
				exit(-1);
			}
			if (existingAttr.retArgTypes != funcTypes) {
				cerr << "[ERROR] Conflicting function definition: " << funcName << endl;
				exit(-1);
			}
			if (existingAttr.defined) {
				cerr << "[ERROR] Duplicate function definition: " << funcName << endl;
				exit(-1);
			}
		}

		SymbolAttributes funcAttr;
		funcAttr.type = tyFUNCTION;
		funcAttr.defined = true;
		funcAttr.retArgTypes = funcTypes;
		stab->addSymbol(funcName, funcAttr);  // This overwrites any declaration

		// Create new symbol table for function body
		string prevFuncName = curFuncName;
		curFuncName = funcName;
		
		// Create function's symbol table
		if (symTabs.find(funcName) == symTabs.end()) {
			symTabs[funcName] = new SymbolTable();
		}
		SymbolTable *funcSymTab = symTabs[funcName];

		// Add parameters to function's symbol table
		for (int i = 1; i < ctx->name().size(); i++) {
			string paramName = ctx->name(i)->getText();
			if (funcSymTab->symbolExists(paramName)) {
				cerr << "[ERROR] Duplicate parameter name: " << paramName << endl;
				exit(-1);
			}
			funcSymTab->addSymbol(paramName, {tyAUTO, true});
		}

		// Visit function body (blockstmt) - directly process statements
		for (auto stmt : ctx->blockstmt()->statement()) {
			visit(stmt);
		}

		// Restore previous function context
		curFuncName = prevFuncName;

		return nullptr;
    }

    any visitBlockstmt(BParser::BlockstmtContext *ctx) override {
		// Make a new scope name for this block
		int nextBlockNum = ++blockCounters[curFuncName];
		
		string blockScopeName;
		if (curFuncName.find("_$") == string::npos && curFuncName.find("_") == string::npos) {
			// Current scope is a function scope
			blockScopeName = curFuncName + "_$" + to_string(nextBlockNum);
		} else {
			// Current scope is already a nested block scope
			blockScopeName = curFuncName + "_" + to_string(nextBlockNum);
		}

		if (symTabs.find(blockScopeName) == symTabs.end()) {
			symTabs[blockScopeName] = new SymbolTable();
		}

		// Save current scope and switch to block scope
		string prevFuncName = curFuncName;
		curFuncName = blockScopeName;

		// Visit all statements in block
		for (auto stmt : ctx->statement()) {
			visit(stmt);
		}

		// Restore previous scope
		curFuncName = prevFuncName;

		return nullptr;
    }

    any visitStatement(BParser::StatementContext *ctx) override {
		visit(ctx->children[0]);
        return nullptr;
    }

    any visitConstant(BParser::ConstantContext *ctx) override {
        
		if (ctx->INT()) return tyINT;
		else if (ctx->REAL()) return tyDOUBLE;
		else if (ctx->STRING()) return tySTRING;
		else if (ctx->BOOL()) return tyBOOL;
		else if (ctx->CHAR()) return tyCHAR;

		cout << "[ERROR] unrecognizable constant is used for initialization: " << ctx->children[0]->getText() << endl;
		exit(-1);
        return nullptr;
    }

};

/*
 * STEP 2. infer type
 */   
class TypeAnalysisVisitor : public BBaseVisitor {
public:
   // infer types for 'auto' variables and functions
   // ...
};

/*
 * STEP 3. print code
 */
class PrintTreeVisitor : public BBaseVisitor {
public:
    any visitProgram(BParser::ProgramContext *ctx) override {
    	// Perform some actions when visiting the program
    	for (int i=0; i< ctx->children.size(); i++) {
      	    visit(ctx->children[i]);
    	}
    	return nullptr;
    }
    
    any visitDirective(BParser::DirectiveContext *ctx) override {
		cout << ctx->SHARP_DIRECTIVE()->getText();
		cout << endl;
        return nullptr;
    }

    any visitDefinition(BParser::DefinitionContext *ctx) override {
		visit(ctx->children[0]);
        return nullptr;
    }

    any visitFuncdef(BParser::FuncdefContext *ctx) override {
		// Handle function definition
        string functionName = ctx->name(0)->getText();
		cout << "auto " << functionName << "(" ;
        // You can retrieve and visit the parameter list using ctx->name(i)
		for (int i=1; i < ctx->name().size(); i++) {
			if (i != 1) cout << ", ";
			cout << "auto " << ctx->name(i)->getText();		
		}
		cout << ")";

		// visit blockstmt
		visit(ctx->blockstmt());
        return nullptr;
    }

    any visitStatement(BParser::StatementContext *ctx) override {
		visit(ctx->children[0]);
        return nullptr;
    }

    any visitAutostmt(BParser::AutostmtContext *ctx) override {
    	// You can retrieve the variable names and constants using ctx->name(i) and ctx->constant(i)
		cout << "auto ";
		for (int i=0, j=0; i < ctx->name().size(); i++) {
			if (i != 0) cout << " ,";
			cout << ctx->name(i)->getText();

			int idx_assn = 1 + i*2 + j*2 + 1;  // auto name (= const)?, name (= const)?, ...
			if (ctx->children[idx_assn]->getText().compare("=") == 0) { 
				if (ctx->constant(j)) {
					cout << " = ";    
					visit(ctx->constant(j));
					j++;
				}
			}
		}
		cout << ";" << endl;
    	return nullptr;
    }

    any visitDeclstmt(BParser::DeclstmtContext *ctx) override {
		// Handle function declaration
        string functionName = ctx->name()->getText();
		cout << "auto " << functionName << "(" ;
        
		// You can retrieve and visit the parameter type list
		for (int i=1; i < ctx->AUTO().size(); i++) {
			if (i != 1) cout << ", ";
			cout << "auto ";		
		}
		cout << ");" << endl;
        return nullptr;
    }

    any visitBlockstmt(BParser::BlockstmtContext *ctx) override {
    	// Perform some actions when visiting a block statement
		cout << "{" << endl;
    	for (auto stmt : ctx->statement()) {
      	    visit(stmt);
    	}
		cout << "}" << endl;
    	return nullptr;
    }

    any visitIfstmt(BParser::IfstmtContext *ctx) override {
		cout << "if (";
		visit(ctx->expr());
		cout << ") " ;

		visit(ctx->statement(0));
		if (ctx->ELSE()) {
	   		cout << endl << "else ";
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
			cout << " (";
			visit(ctx->expression());
			cout << ")";
		}
		cout << ";" << endl;
        return nullptr;
    }

    any visitNullstmt(BParser::NullstmtContext *ctx) override {
		cout << ";" << endl;
        return nullptr;
    }

    any visitExpr(BParser::ExprContext *ctx) override {
		// unary operator
        if(ctx->atom()) {
            if (ctx->PLUS()) cout << "+";
            else if (ctx->MINUS()) cout << "-";
	    	else if (ctx->NOT()) cout << "!";
	    	visit(ctx->atom()); 
        }
		// binary operator
		else if (ctx->MUL() || ctx->DIV() || ctx->PLUS() || ctx->MINUS() || 
		 		ctx->GT() || ctx->GTE() || ctx->LT() || ctx->LTE() || ctx->EQ() || ctx->NEQ() ||
		 		ctx->AND() || ctx->OR() ) {
	    	visit(ctx->expr(0));
	    	cout << " " << ctx->children[1]->getText() << " "; // print binary operator
	    	visit(ctx->expr(1));
		}
		// ternary operator
		else if (ctx->QUEST()) {
			visit(ctx->expr(0));
			cout << " ? ";
			visit(ctx->expr(1));
			cout << " : ";
			visit(ctx->expr(2));
		}
		else {
			int lineNum = ctx->getStart()->getLine();
			cerr << endl << "[ERROR] visitExpr: unrecognized ops in Line " << lineNum << " --" << ctx->children[1]->getText() << endl;
			exit(-1); // error
        }	
        return nullptr;
    }
   
    any visitAtom(BParser::AtomContext *ctx) override {
		if (ctx->expression()) { // ( expression )
			cout << "(";
			visit(ctx->expression());
			cout << ")";
		}
		else	// name | constant | funcinvocation
			visit(ctx->children[0]);
        return nullptr;
    }
    
    any visitExpression(BParser::ExpressionContext *ctx) override {
        if (ctx->ASSN()) { // assignment
	   		visit(ctx->name());
	  		 cout << " = ";
		}
		visit(ctx->expr());
        return nullptr;
    }

    any visitFuncinvocation(BParser::FuncinvocationContext *ctx) override {
		cout << ctx->name()->getText() << "(";
		for (int i=0; i < ctx->expr().size(); i++) {
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
    std::ifstream stream;
    stream.open(argv[1]);
    if (stream.fail()) {
        cerr << argv[1] << " : file open fail\n";
        exit(0);
    }

	//cout << "/*-- B2C ANTLR visitor --*/\n";

	ANTLRInputStream inputStream(stream);
	BLexer lexer(&inputStream);
	CommonTokenStream tokenStream(&lexer);
	BParser parser(&tokenStream);
	ParseTree* tree = parser.program();

	// STEP 1. visit parse tree and build symbol tables for functions (PA#1)
	cout << endl << "/*** STEP 1. BUILD SYM_TABS *************" << endl;
	SymbolTableVisitor SymtabTree;
	SymtabTree.visit(tree);
	cout <<         " ***    end of step 1       *************/" << endl;

	// STEP 2. visit parse tree and perform type inference for 'auto' typed variables and functions (PA#2)
	cout << endl << "/*** STEP 2. ANALYZE TYPES  *************" << endl;
	TypeAnalysisVisitor AnalyzeTree;
	AnalyzeTree.visit(tree);
	cout <<         " ***    end of step 2       *************/" << endl;

	// STEP 3. visit parse tree and print out C code with correct types
	cout << endl << "/*** STEP 3. TRANSFORM to C *************/" << endl;
	PrintTreeVisitor PrintTree;
	PrintTree.visit(tree);

	return 0;
}
