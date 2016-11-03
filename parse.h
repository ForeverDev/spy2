#ifndef PARSE_H
#define PARSE_H

#include "spyconf.h"
#include "lex.h"
	
typedef struct ParseState ParseState;
typedef struct ParseOptions ParseOptions;
typedef struct ExpNode ExpNode;
typedef struct TreeType TreeType;
typedef struct TreeStruct TreeStruct;
typedef struct TreeTypeList TreeTypeList;
typedef struct BinaryOp BinaryOp;
typedef struct UnaryOp UnaryOp;
typedef struct FuncCall FuncCall;
typedef struct TypeCast TypeCast;
typedef struct TreeNode TreeNode;
typedef struct TreeBlock TreeBlock;
typedef struct TreeStatement TreeStatement;
typedef struct TreeIf TreeIf;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFor TreeFor;
typedef struct TreeDecl TreeDecl;
typedef struct TreeFunction TreeFunction;
typedef struct TreeVariable TreeVariable;
typedef struct TreeVariableList TreeVariableList;
typedef struct LiteralList LiteralList;
typedef struct CallState CallState;
typedef enum TreeNodeType TreeNodeType;

enum TreeNodeType {
	NODE_IF = 1,
	NODE_FOR = 2,
	NODE_WHILE = 3,
	NODE_STATEMENT = 4,
	NODE_BLOCK = 5,
	NODE_FUNCTION = 6,
	NODE_RETURN = 7,
	NODE_BREAK = 8,
	NODE_CONTINUE = 9
};

struct LiteralList {
	char* literal;
	LiteralList* next;
};	

struct TreeType {
	char* type_name;
	unsigned int plevel; /* depth of pointer */
	unsigned int size; /* number of bytes needed to store */
	uint16_t modifier;
	int is_generic; /* if it's a generic type, the type is not yet known... */
	int generic_index; /* the index in the function generic list */
	TreeStruct* sval; /* NULL if it's not a struct */
};

struct TreeStruct {
	TreeVariableList* fields;
	int initialized;
};

struct TreeVariable {
	char* identifier;
	TreeType* datatype;	
};

struct TreeTypeList {
	TreeType* datatype;
	TreeTypeList* next;
};

struct TreeVariableList {
	TreeVariable* variable;
	TreeVariableList* next;
	TreeVariableList* prev;
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
	TreeType* datatype;
	ExpNode* operand;
};
struct FuncCall {
	TreeFunction* func;
	ExpNode* argument; /* entire arg list is one expression */
	TreeTypeList* generic_list;
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

struct TreeFunction {
	char* identifier;
	uint32_t modifiers;
	int implemented;
	int nparams;
	LiteralList* generics;
	int ngenerics;
	TreeVariableList* params;
	TreeType* return_type;	
	TreeNode* child;
};

struct TreeNode {
	TreeNodeType type;
	union {
		TreeIf* ifval;
		TreeFor* forval;
		TreeWhile* whileval;
		ExpNode* stateval; /* also used for return */
		TreeBlock* blockval;
		TreeFunction* funcval;
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
		EXP_CAST,
		EXP_FUNC_CALL
	} type;
	union {
		spy_integer ival;
		spy_float fval;
		spy_string sval; 
		char* idval;
		TreeType* tval; /* datatype (e.g. cast, template) */
		UnaryOp* uval;
		BinaryOp* bval;
		TypeCast* cval;
		FuncCall* fcval;
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

struct CallState {
	TreeNode* block;
	TreeNode* function;
	TreeTypeList* generic_set;
	CallState* next;
	CallState* prev;
};

struct ParseState {
	const char* filename;
	unsigned int total_lines;
	Token* token;
	Token* end_mark; /* marks the end of an expression */ 
	TreeTypeList* defined_types;
	TreeNode* to_append;
	TreeNode* current_block;
	TreeNode* current_function;
	TreeNode* current_loop;
	TreeNode* root_block;
	ParseOptions* options;
	TreeTypeList* generic_set;
	TreeType* type_integer;
	TreeType* type_float;
	TreeType* type_byte;
	TreeType* type_void;
	CallState* call_state;
};

ParseState* generate_tree(LexState*, ParseOptions*);

#endif