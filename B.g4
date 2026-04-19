/* 
   Minimized C language
   All types are 'auto'
*/

grammar B;

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

// list of variables with type 'auto':   e.g., auto a, b = 10;
autostmt
   : AUTO name (ASSN constant)? (',' name (ASSN constant)?)* SEMI 
   ;

// function declatation with 'auto' return type and arguments:  e.g., auto fn(auto, auto);
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

expressionstmt
   : expression SEMI 
   ;

returnstmt
   : RETURN ( expression | '(' expression ')')? SEMI 
   ;

nullstmt
   : SEMI 
   ;

expr
   : (PLUS | MINUS | NOT)? atom
   | expr (MUL | DIV) expr
   | expr (PLUS | MINUS) expr
   | expr (GT | GTE | LT | LTE) expr
   | expr (EQ | NEQ) expr
   | expr AND expr
   | expr OR expr
   | expr QUEST expr COLON expr
   ;

atom
   : name
   | constant
   | '(' expression ')'
   | funcinvocation
   ;

expression
   : (name ASSN)? expr
   ;

funcinvocation
   : name '(' ( expr (',' expr)* )? ')'
   ;

constant
   : INT
   | REAL
   | STRING
   | BOOL
   | CHAR
   ;

name
   : NAME
   ;

/* Lexer Rules */

AUTO:  'auto' ;

PLUS:  '+' ;
MINUS: '-' ;
MUL:   '*' ;
DIV:   '/' ;
NOT:   '!' ;
GT:    '>' ;
GTE:   '>=' ;
LT:    '<' ;
LTE:   '<=' ;
EQ:    '==' ;
NEQ:   '!=' ;
AND:   '&&' ;
OR:    '||' ;
QUEST: '?' ;
COLON: ':' ;
SEMI:  ';' ;
 
IF:     'if' ;
ELSE:   'else' ;
WHILE:  'while' ;
RETURN: 'return' ;
ASSN:   '=' ;

BOOL
   : 'false'|'true'
   ;

NAME
   : [a-zA-Z] [a-zA-Z0-9_]*
   ;

INT
   : [0-9]+
   ;

REAL
   : [0-9]+ '.' [0-9]*
   ;

STRING
   : '"' ~["\r\n]* '"'
   ;

CHAR
   : '\'' . '\''
   ;

SHARP_DIRECTIVE
   : '#' ~[\r\n]* 
   ;

BLOCKCOMMENT
   : '/*' .*? '*/' -> skip
   ;

LINECOMMENT
    :   '//' ~[\r\n]* -> skip
    ;

WS
   : [ \t\r\n] -> skip
   ;

