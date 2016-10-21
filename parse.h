#ifndef PARSE_H
#define PARSE_H

#include "spyconf.h"
#include "lex.h"
	
typedef struct ParseState ParseState;
typedef struct ExpNode ExpNode;
typedef struct SpyType SpyType;
typedef struct BinaryOp BinaryOp;
typedef struct UnaryOp UnaryOp;

struct SpyType {
	char* type_name;
	unsigned int plevel; /* depth of pointer */
	unsigned int size; /* number of bytes needed to store */
	uint16_t modifier;
};

struct BinaryOp {
	TokenType type;
	ExpNode* left;
	ExpNode* right;	
};

struct UnaryOp {
	TokenType type;
	ExpNode* operand;
};

struct ExpNode {
	/* only acceptable things in an expression are
	 * binary operators, unary operators, string literals,
	 * number literals, datatypes (e.g. cast) and variables
	 */
	ExpNode* parent;
	/* side tells whether the node is on the left side or
	 * the right side of the parent binary operator..
	 *	1 = left
	 *	2 = right 
	 */
	unsigned int side;
	enum ExpNodeType {
		NODE_BINOP,
		NODE_UNOP,
		NODE_OPENPAR,  /* only used for shunting yard */
		NODE_CLOSEPAR, /* only used for shunting yard */
		NODE_STRING,
		NODE_INTEGER,
		NODE_FLOAT,
		NODE_DATATYPE,
		NODE_LOCAL,
		NODE_IDENTIFIER
	} type;
	union {
		spy_integer ival;
		spy_float fval;
		spy_string sval; 
		char* idval;
		SpyType* tval; /* datatype (e.g. cast, template) */
		UnaryOp* uval;
		BinaryOp* bval;
	};
};

struct ParseState {
	Token* token;
};

ParseState* generate_tree(Token*);

#endif
