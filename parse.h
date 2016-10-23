#ifndef PARSE_H
#define PARSE_H

#include "spyconf.h"
#include "lex.h"
	
typedef struct ParseState ParseState;
typedef struct ParseOptions ParseOptions;
typedef struct ExpNode ExpNode;
typedef struct SpyType SpyType;
typedef struct SpyTypeList SpyTypeList;
typedef struct BinaryOp BinaryOp;
typedef struct UnaryOp UnaryOp;
typedef struct TypeCast TypeCast;
typedef struct TreeNode TreeNode;
typedef struct TreeBlock TreeBlock;
typedef struct TreeStatement TreeStatement;
typedef struct TreeIf TreeIf;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFor TreeFor;
typedef struct TreeVariable TreeVariable;
typedef struct TreeVariableList TreeVariableList;
typedef enum TreeNodeType TreeNodeType;

enum TreeNodeType {
	NODE_IF,
	NODE_FOR,
	NODE_WHILE,
	NODE_STATEMENT,
	NODE_BLOCK
};

struct SpyType {
	char* type_name;
	unsigned int plevel; /* depth of pointer */
	unsigned int size; /* number of bytes needed to store */
	uint16_t modifier;
};

struct TreeVariable {
	char* identifier;
	SpyType* datatype;	
};

struct SpyTypeList {
	SpyType* datatype;
	SpyTypeList* next;
	SpyTypeList* prev;
};

struct TreeVariableList {
	TreeVariable* variable;
	TreeVariableList* next;
};

/* expresstion structs */
struct BinaryOp {
	TokenType type;
	ExpNode* left;
	ExpNode* right;	
};
struct UnaryOp {
	TokenType type;
	ExpNode* operand;
};
struct TypeCast {
	SpyType* datatype;
	ExpNode* operand;
};

struct TreeIf {
	ExpNode* condition;
	TreeNode* child;
};

struct TreeWhile {
	ExpNode* condition;
	TreeNode* child;
};

struct TreeFor {
	ExpNode* initializer;
	ExpNode* condition;
	ExpNode* statement;
	TreeVariable* var; /* optional declaration in initializer */
	TreeNode* child;
};

struct TreeBlock {
	TreeNode* child;
	TreeVariableList* locals;
};

struct TreeNode {
	TreeNodeType type;
	union {
		TreeIf* ifval;
		TreeFor* forval;
		TreeWhile* whileval;
		ExpNode* stateval;
		TreeBlock* blockval;
	};
	TreeNode* next;
	TreeNode* prev;
	TreeNode* parent;
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
		EXP_BINOP,
		EXP_UNOP,
		EXP_OPENPAR,  /* only used for shunting yard */
		EXP_CLOSEPAR, /* only used for shunting yard */
		EXP_STRING,
		EXP_INTEGER,
		EXP_FLOAT,
		EXP_BYTE,
		EXP_DATATYPE,
		EXP_LOCAL,
		EXP_IDENTIFIER,
		EXP_CAST
	} type;
	union {
		spy_integer ival;
		spy_float fval;
		spy_string sval; 
		char* idval;
		SpyType* tval; /* datatype (e.g. cast, template) */
		UnaryOp* uval;
		BinaryOp* bval;
		TypeCast* cval;
	};
};

struct ParseOptions {
	enum ParseOptimizationLevel {
		OPT_ZERO = 0,	/* no optimization */
		OPT_ONE = 1,	/* optimize trivial arithmetic */
		OPT_TWO = 2,	/* optimize branching */
		OPT_THREE = 3	/* TBD */
	} opt_level;
};

struct ParseState {
	const char* filename;
	unsigned int total_lines;
	Token* token;
	Token* end_mark; /* marks the end of an expression */ 
	SpyTypeList* defined_types;
	TreeNode* to_append;
	TreeNode* current_block;
	TreeNode* root_block;
	ParseOptions* options;
	const SpyType* type_integer;
	const SpyType* type_float;
	const SpyType* type_byte;
};

ParseState* generate_tree(LexState*, ParseOptions*);

#endif
