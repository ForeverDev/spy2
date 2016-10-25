#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parse.h"

#define LEAF_LEFT (1)
#define LEAF_RIGHT (2)

#define MOD_STATIC (0x1 << 0)
#define MOD_CONST (0x1 << 1)
#define MOD_VOLATILE (0x1 << 2)
#define MOD_CFUNC (0x1 << 3)
#define MOD_COUNT 4

typedef struct ExpStack ExpStack;
typedef struct OperatorInfo OperatorInfo;
typedef struct ModifierInfo ModifierInfo;

/* debug functions */
static void print_datatype(TreeType*);
static void print_declaration(TreeVariable*);
static void print_node(TreeNode*, int);
static void print_expression(ExpNode*, int);
static void parse_error(ParseState*, const char*, ...); 
static void parse_die(ParseState*, const char*, va_list);

/* optimization functions */
static void optimize_branching(ParseState*, TreeNode*);
static void optimize_tree_arith(ParseState*, ExpNode*);
static int optimize_arith_available(ParseState*, ExpNode*);

/* functions that tell you whether or not you're looking
 * at a certain type of expression */
static int matches_datatype(ParseState*);
static int matches_declaration(ParseState*);
static int matches_function(ParseState*);

/* parsing functions */
static TreeType* parse_datatype(ParseState*);
static ExpNode* parse_expression(ParseState*);
static TreeNode* parse_statement(ParseState*);
static TreeVariable* parse_declaration(ParseState*);
static void parse_if(ParseState*);
static void parse_while(ParseState*);
static void parse_for(ParseState*);
static void parse_function(ParseState*);
static void parse_return(ParseState*);
static void parse_break(ParseState*);
static void parse_continue(ParseState*);

/* expression stack functions */
static void expstack_push(ExpStack*, ExpNode*);
static void expstack_print(ExpStack*);
static ExpNode* expstack_pop(ExpStack*);
static ExpNode* expstack_top(ExpStack*);
static char* expstack_tostring(ExpStack*);

/* misc functions */
static Token* token(ParseState*);
static int on(ParseState*, int, ...);
static void next(ParseState*, int);
static void register_datatype(ParseState*, TreeType*);
static void register_local(ParseState*, TreeVariable*);
static TreeType* get_datatype(ParseState*, const char*);
static void make_sure(ParseState*, TokenType, const char*, ...);
static void mark_expression(ParseState*, TokenType, TokenType);
static TreeNode* new_node(ParseState*, TreeNodeType);
static void append(ParseState*, TreeNode*);
static int is_keyword(const char*);
static int get_modifier(const char*);
static Token* peek(ParseState*, int);
static void typcheck_tree(ParseState*, ExpNode*);
static int exact_datatype(TreeType*, TreeType*);
static char* tostring_datatype(TreeType*);

struct OperatorInfo {
	unsigned int pres;
	enum OperatorAssoc {
		ASSOC_LEFT = 1,
		ASSOC_RIGHT = 2
	} assoc;
	enum OperatorType {
		OP_UNARY = 1,
		OP_BINARY = 2
	} optype;
};

struct ModifierInfo {
	const char* identifier;
	uint32_t bitflag;
};

/* only used to generate a postfix representation
 * of an expression */
struct ExpStack {
	ExpNode* node;
	ExpStack* next;
	ExpStack* prev;
};

static const ModifierInfo modifiers[4] = {
	{"static", MOD_STATIC},
	{"const", MOD_CONST},
	{"volatile", MOD_VOLATILE},
	{"cfunc", MOD_CFUNC}
};

static const char* keywords[32] = {
	"if", "for", "while", "func", "return",
	"continue", "break"
};

/* list of operators that can be optimized */
static const int optimizable[255] = {
	[TOK_PLUS] = 1,
	[TOK_HYPHON] = 1,
	[TOK_ASTER] = 1,
	[TOK_FORSLASH] = 1,
	[TOK_SHL] = 1,
	[TOK_SHR] = 1,
	[TOK_GT] = 1,
	[TOK_LT] = 1,
	[TOK_GE] = 1,
	[TOK_LE] = 1,
	[TOK_EQ] = 1,
	[TOK_ASSIGN] = 1,
	[TOK_INCBY] = 1,
	[TOK_DECBY] = 1,
	[TOK_MULBY] = 1,
	[TOK_DIVBY] = 1,
	[TOK_MODBY] = 1,
	[TOK_SHLBY] = 1,
	[TOK_SHRBY] = 1,
	[TOK_ANDBY] = 1,
	[TOK_ORBY] = 1,
	[TOK_XORBY] = 1
};

/* called by parse_error and make_sure */
static void
parse_die(ParseState* P, const char* format, va_list list) {
	printf("\n\n*** SPYRE COMPILE-TIME ERROR ***\n\n");
	printf("\tmessage: ");
	vprintf(format, list);
	/* resort to last line if there is no token */
	printf("\n\tline:    %d\n", P->token ? P->token->line : P->total_lines);
	printf("\tfile:    %s\n\n\n", P->filename);
	exit(1);
}

static void
parse_error(ParseState* P, const char* format, ...) {
	va_list list;
	va_start(list, format);
	parse_die(P, format, list);
	va_end(list);
}

static Token*
peek(ParseState* P, int amount) {
	Token* at = P->token;
	for (int i = 0; i < amount; i++) {
		if (!at) return NULL;
		at = at->next;
	}
	return at;
}

static void
make_sure(ParseState* P, TokenType type, const char* err, ...) {
	va_list list;
	va_start(list, err);
	if (!P->token || P->token->type != type) {
		parse_die(P, err, list);
	}
	va_end(list);
}

static int 
get_modifier(const char* word) {
	for (int i = 0; i < MOD_COUNT; i++) {
		if (!strcmp(word, modifiers[i].identifier)) {
			return modifiers[i].bitflag;
		}
	}
	return 0;
}

static int
is_keyword(const char* word) {
	for (const char** i = &keywords[0]; *i; i++) {
		if (!strcmp(word, *i)) {
			return 1;
		}
	}
	return 0;
}

static int
exact_datatype(TreeType* a, TreeType* b) {
	if (strcmp(a->type_name, b->type_name)) return 0;
	if (a->modifier != b->modifier) return 0;
	if (a->plevel != b->plevel) return 0;
	return 1;
}
	
static void
expstack_push(ExpStack* stack, ExpNode* node) {
	if (!stack->node) {
		stack->node = node;
		stack->next = NULL;
		stack->prev = NULL;
		return;
	}
	ExpStack* i;
	for (i = stack; i->next; i = i->next);
	ExpStack* push = malloc(sizeof(ExpStack));
	push->node = node;
	push->next = NULL;
	push->prev = i;
	i->next = push;
}

static ExpNode*
expstack_pop(ExpStack* stack) {
	if (!stack->next) {
		ExpNode* ret = stack->node;
		stack->node = NULL;
		return ret;
	}
	ExpStack* i;
	for (i = stack; i->next; i = i->next);
	if (i->prev) {
		i->prev->next = NULL;
	}
	ExpNode* ret = i->node;
	free(i);
	return ret;
}

static ExpNode*
expstack_top(ExpStack* stack) {
	if (!stack->next) {
		return stack->node;
	}
	ExpStack* i;
	for (i = stack; i->next; i = i->next);
	return i->node;
}

static char*
expstack_tostring(ExpStack* stack) {
	ExpNode* node = stack->node;
	char* buf;
	if (!node) {
		buf = malloc(2);
		strcpy(buf, "?");
		return buf;
	}
	buf = malloc(128); /* 128 _should_ be enough space */
	switch (node->type) {
		case EXP_BINOP:
			sprintf(buf, "%s", tt_to_word(node->bval->type)); 
			break;
		case EXP_UNOP:
			sprintf(buf, "%s", tt_to_word(node->uval->type)); 
			break;
		case EXP_STRING:
			sprintf(buf, "%s", node->sval);
			break;
		case EXP_INTEGER:
			sprintf(buf, "%lld", node->ival);
			break;
		case EXP_FLOAT:
			sprintf(buf, "%lf", node->fval);
			break;
		case EXP_IDENTIFIER:
			sprintf(buf, "%s", node->idval);
			break;
		case EXP_CAST:
			sprintf(buf, " ");
			break;
		default:
			break;
	}
	return buf;
}

static void
expstack_print(ExpStack* stack) {
	for (ExpStack* i = stack; i; i = i->next) {
		char* tostring = expstack_tostring(i);
		printf("%s ", tostring);
		free(tostring);
	}
	printf("\n");
}

static int
on(ParseState* P, int num, ...) {
	va_list list;
	va_start(list, num);
	
	for (int i = 0; i < num; i++) {
		Token* check = va_arg(list, Token*);
		if (!token(P) || !check) return 0;
		if (P->token->type == check->type) {
			va_end(list);
			return 1;
		}
	}

	va_end(list);
	return 0;
}

static inline Token* 
token(ParseState* P) {
	return P->token;
}

static void
next(ParseState* P, int increase) {
	for (int i = 0; i < increase && token(P); i++) {
		P->token = P->token->next;
	}
}

static void
mark_expression(ParseState* P, TokenType inc, TokenType dec) {
	int count = 1;
	Token* search = P->token;
	for (Token* i = P->token; i; i = i->next) {
		if (i->type == inc) {
			count++;
		} else if (i->type == dec) {
			count--;
		}
		if (i->type == TOK_CLOSECURL || i->type == TOK_OPENCURL) {
			P->token = i;
			parse_error(P, "unexpected token while parsing: '%s'", i->word);
		} else if (is_keyword(i->word)) {
			P->token = i;
			parse_error(P, "unexpected keyword while parsing expression: '%s'", i->word);
		}
		/* if the count is 0, mark the token as the end of the expression */
		if (count == 0) {
			P->end_mark = i;
			return;
		}
	}
	parse_error(P, "unexpected EOF when parsing expression");
}

static void
register_datatype(ParseState* P, TreeType* type) {
	if (!P->defined_types->datatype) {
		P->defined_types->datatype = type;
		return;
	}
	TreeTypeList* list;
	for (list = P->defined_types; list->next; list = list->next);
	TreeTypeList* new = malloc(sizeof(TreeTypeList));
	new->datatype = type;
	new->next = NULL;
	new->prev = list;
	list->next = new;
}

static void
register_local(ParseState* P, TreeVariable* var) {
	TreeBlock* block = P->current_block->blockval;
	if (!block->locals) {
		block->locals = malloc(sizeof(TreeVariableList));
		block->locals->variable = var;
		block->locals->next = NULL;
		return;
	}	
	TreeVariableList* i;
	for (i = block->locals; i->next; i = i->next);
	TreeVariableList* new = malloc(sizeof(TreeVariableList));
	new->variable = var;
	new->next = NULL;
	i->next = new;
}

static TreeType*
get_datatype(ParseState* P, const char* type_name) {
	for (TreeTypeList* i = P->defined_types; i && i->datatype; i = i->next) {
		if (!strcmp(i->datatype->type_name, type_name)) {
			return i->datatype;
		}
	}
	return NULL;
}

static char*
tostring_datatype(TreeType* type) {
	char* buf = calloc(1, 256);
	for (int i = 0; i < 4; i++) {
		if ((type->modifier >> i) & 0x1) {
			sprintf(buf + strlen(buf), "%s ", modifiers[i].identifier);
		}
	}
	sprintf(buf + strlen(buf), "%s", type->type_name);
	for (int i = 0; i < type->plevel; i++) {
		sprintf(buf + strlen(buf), "^");	
	}
	return buf;
}

static void
print_datatype(TreeType* type) {
	char* buf = tostring_datatype(type);
	printf("%s", buf);
	free(buf);
	return;
}

static void
print_declaration(TreeVariable* var) {
	printf("%s: ", var->identifier);
	print_datatype(var->datatype);
}

#define INDENT(n) for (int _=0; _<(n); _++) printf("    ")

static void
print_node(TreeNode* node, int indent) {
	if (!node) return;
	INDENT(indent);
	switch (node->type) {
		case NODE_STATEMENT:
			printf("STATEMENT: [\n");
			print_expression(node->stateval, indent + 1);	
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_BLOCK:
			printf("BLOCK: [\n");
			INDENT(indent + 1);
			printf("LOCALS: [\n");
			for (TreeVariableList* i = node->blockval->locals; i; i = i->next) {
				INDENT(indent + 2);
				print_declaration(i->variable);
				printf("\n");
			}
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			for (TreeNode* i = node->blockval->child; i; i = i->next) {
				print_node(i, indent + 2);
			}
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_IF:
			printf("IF: [\n");
			INDENT(indent + 1);
			printf("CONDITION: [\n");
			print_expression(node->ifval->condition, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_node(node->ifval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_WHILE:
			printf("WHILE: [\n");
			INDENT(indent + 1);
			printf("CONDITION: [\n");
			print_expression(node->whileval->condition, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_node(node->whileval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_FOR:
			printf("FOR: [\n");
			INDENT(indent + 1);
			printf("INITIALIZER: [\n");
			print_expression(node->forval->initializer, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CONDITION: [\n");
			print_expression(node->forval->condition, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("STATEMENT: [\n");
			print_expression(node->forval->statement, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_node(node->forval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_FUNCTION:
			printf("FUNCTION: [\n");
			INDENT(indent + 1);
			printf("IDENTIFIER: %s\n", node->funcval->identifier);
			INDENT(indent + 1);
			printf("RETURN TYPE: ");
			print_datatype(node->funcval->return_type);
			printf("\n");
			INDENT(indent + 1);
			printf("PARAMETERS: [\n");
			for (TreeVariableList* i = node->funcval->params; i; i = i->next) {
				INDENT(indent + 2);
				print_declaration(i->variable);
				printf("\n");
			}
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_node(node->funcval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_RETURN:
			printf("RETURN: [\n");
			print_expression(node->stateval, indent + 1);
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_BREAK:
			printf("BREAK\n");
			break;
		case NODE_CONTINUE:
			printf("CONTINUE\n");
			break;
	}	
}

static void
print_expression(ExpNode* tree, int indent) {
	if (!tree) return;
	INDENT(indent);
	switch (tree->type) {
		case EXP_BINOP:
			printf("%s\n", tt_to_word(tree->bval->type));
			print_expression(tree->bval->left, indent + 1);	
			print_expression(tree->bval->right, indent + 1);	
			break;
		case EXP_UNOP:
			printf("%s\n", tt_to_word(tree->uval->type));
			print_expression(tree->uval->operand, indent + 1);	
			break;
		case EXP_INTEGER:
			printf("%lld\n", tree->ival);
			break;
		case EXP_FLOAT:
			printf("%lf\n", tree->fval);
			break;
		case EXP_IDENTIFIER:
			printf("%s\n", tree->idval);
			break;
		case EXP_DATATYPE:
			print_datatype(tree->tval);
			printf("\n");
			break;
		case EXP_CAST:
			printf("(");
			print_datatype(tree->cval->datatype);
			printf(")\n");
			print_expression(tree->cval->operand, indent + 1);
			break;
	}
}

/* tells whether or not the parser is looking at a function definition */
/* syntax:
 *	 <identifier> ":" <modifier>* "(" <declaration>* ")" -> <datatype> "{"
 */
static int
matches_function(ParseState* P) {
	Token* at = P->token;
	if (!at || at->type != TOK_IDENTIFIER) return 0;	
	at = at->next;
	if (!at || at->type != TOK_COLON) return 0;
	at = at->next;
	/* optional function modifiers */
	while (at && get_modifier(at->word)) {
		at = at->next;
	}
	if (!at || at->type != TOK_OPENPAR) return 0;
	return 1;
}

/* tells whether or not the parser is looking at a datatype */
static int
matches_datatype(ParseState* P) {
	Token* at = P->token;
	while (get_modifier(at->word)) {
		at = at->next;
	}
	return get_datatype(P, at->word) != NULL;
}

static int
matches_declaration(ParseState* P) {
	Token* start = P->token;
	Token* at = P->token;
	if (!at || at->type != TOK_IDENTIFIER) return 0;
	at = at->next;
	if (!at || at->type != TOK_COLON) return 0;
	at = at->next;
	P->token = at; /* will reset to start after calling matches_datatype */
	int is_datatype = matches_datatype(P);
	P->token = start;
	return is_datatype;
}

static TreeNode*
new_node(ParseState* P, TreeNodeType type) {
	TreeNode* node = malloc(sizeof(TreeNode));
	switch (type) {
		case NODE_IF:
		case NODE_FOR:
		case NODE_WHILE:
		case NODE_STATEMENT:
			node->type = NODE_STATEMENT;
			node->stateval = NULL; /* to be assigned */
			break;
	}
	return node;
}

/* appends a node to whatever is needed... e.g. if
 * the most recent thing is an if statement, the 
 * node will be attached to that.  Else, it is
 * appended into the current block */
static void
append(ParseState* P, TreeNode* node) {
	/* stick it onto the node if it is appendable */
	node->next = NULL;
	node->prev = NULL;
	node->parent = NULL;
	if (P->to_append) {
		switch (P->to_append->type) {
			case NODE_IF:
				P->to_append->ifval->child = node;	
				break;
			case NODE_WHILE:
				P->to_append->whileval->child = node;
				break;
			case NODE_FOR:
				P->to_append->forval->child = node;
				break;
			case NODE_FUNCTION:
				P->to_append->funcval->child = node;
				break;
		}
		node->parent = P->to_append;
	/* otherwise just throw it into the current block */
	} else {
		TreeBlock* current_block = P->current_block->blockval;
		if (!current_block->child) {
			current_block->child = node;
		} else {
			TreeNode* tail;
			for (tail = current_block->child; tail->next; tail = tail->next);
			tail->next = node;
			node->prev = tail;
		}
		node->parent = P->current_block;
		node->next = NULL;
	}
	/* here is where we make sure that return is only used inside of a function
	 * and that continue and break are only used inside of loops */
	if (node->type == NODE_RETURN) {
		int found_func = 0;
		for (TreeNode* i = node->parent; i; i = i->parent) {
			if (i->type == NODE_FUNCTION) {
				found_func = 1;
				break;
			}
		}
		if (!found_func) {
			parse_error(P, "attempt to use 'return' outside of a function");
		}
	} else if (node->type == NODE_CONTINUE || node->type == NODE_BREAK) {
		int found_loop = 0;
		for (TreeNode* i = node->parent; i; i = i->parent) {
			if (i->type == NODE_WHILE || i->type == NODE_FOR) {
				found_loop = 1;
				break;
			}
		}
		if (!found_loop) {
			parse_error(
				P, 
				"attempt to use '%s' outside of a loop",
				node->type == NODE_CONTINUE ? "continue" : "break"	
			);
		}
	}
	if (node->type == NODE_IF 
		|| node->type == NODE_WHILE 
		|| node->type == NODE_FOR
		|| (node->type == NODE_FUNCTION && node->funcval->implemented)
	) {
		P->to_append = node;
	} else {
		P->to_append = NULL;
	}	
	if (node->type == NODE_BLOCK) {
		P->current_block = node;
	}
}

static TreeVariable*
get_local(ParseState* P, const char* identifier) {
	for (TreeNode* i = P->current_block; i; i = i->parent) {
		if (i->type != NODE_BLOCK) continue;
		for (TreeVariableList* j = i->blockval->locals; j; j = j->next) {
			if (!strcmp(j->variable->identifier, identifier)) {
				return j->variable;
			}
		}
	}
	return NULL;
}

static const TreeType*
tree_datatype(ParseState* P, ExpNode* tree) {
	if (tree->type == EXP_BINOP) {
		const TreeType* left = tree_datatype(P, tree->bval->left);
		const TreeType* right = tree_datatype(P, tree->bval->right);
	} else if (tree->type == EXP_UNOP) {
		const TreeType* operand = tree_datatype(P, tree->uval->operand);
	} else if (tree->type == EXP_IDENTIFIER) {
		TreeVariable* var = get_local(P, tree->idval);	
		if (!var) {
			parse_error(P, "undeclared identifier '%s'", tree->idval);
		}
		return var->datatype;
	} else if (tree->type == EXP_INTEGER) {
		return P->type_integer;	 
	} else if (tree->type == EXP_FLOAT) {
		return P->type_float;
	} else if (tree->type == EXP_BYTE) {
		return P->type_byte;
	}
	return NULL;
}

static void
typecheck_expression(ParseState* P, ExpNode* tree) {
	
}

/* optimizes trivial branching, e.g.
 * if (0) {
 * 
 * }
 * will be ignored and
 * if (1) {
 *
 * }
 * will not test the condition
 */

/* TODO FIX THIS, DOESN'T WORK IN ALL CASES!!!! */
static void
optimize_branching(ParseState* P, TreeNode* node) {
	if (!node) return;
	switch (node->type) {
		case NODE_BLOCK:
			for (TreeNode* i = node->blockval->child; i; i = i->next) {
				optimize_branching(P, i);
			}
			break;
		case NODE_IF:
			if (node->ifval->condition->type == EXP_INTEGER 
				|| node->ifval->condition->type == EXP_FLOAT
			) {	
				TreeNode* parent = node->parent;	
				/* TODO make a remove_from_parent function */
				/* condition is 1, remove test */
				if (*(uint64_t *)&node->ifval->condition->ival) {
					switch (parent->type) {
						case NODE_IF:
							parent->ifval->child = node->ifval->child;
							break;	
						case NODE_WHILE:
							parent->whileval->child = node->whileval->child;
							break;	
						case NODE_BLOCK:
							node->ifval->child->parent = parent;
							if (node->prev) {
								if (node->next) {
									node->ifval->child->prev = node->prev;
									node->ifval->child->next = node->next;
									node->prev->next = node->ifval->child;
									node->next->prev = node->ifval->child;
								} else {
									node->ifval->child->prev = node->prev;
									node->ifval->child->next = NULL;
									node->prev->next = node->ifval->child;
								}
							} else {
								if (node->next) {
									parent->blockval->child = node->ifval->child;
									node->ifval->child->next = node->next;
									node->ifval->child->prev = NULL;
								} else {
									parent->blockval->child = node->ifval->child;
									node->ifval->child->next = NULL;
									node->ifval->child->prev = NULL;
								}
							}
							break;
						
					}
				/* condition is 0, remove completely */
				} else {
					switch (parent->type) {
						case NODE_IF:
							parent->ifval->child = NULL;	
							break;
						case NODE_WHILE:
							parent->whileval->child = NULL;
							break;
						case NODE_BLOCK: 
							if (node->prev) {
								if (node->next) {
									node->prev->next = node->next;
								} else {
									node->prev->next = NULL;
								}
							} else {
								if (node->next) {
									parent->blockval->child = node->next;
								} else {
									parent->blockval->child = NULL;
								}
							}
							break;
						
					}
				}
			}
			optimize_branching(P, node->ifval->child);
			break;
	}
}

static int
optimize_arith_available(ParseState* P, ExpNode* tree) {
	switch (tree->type) {
		case EXP_BINOP: {
			ExpNode* left = tree->bval->left;
			ExpNode* right = tree->bval->right;
			if (!optimizable[tree->bval->type]) {
				return 0;
			}
			if ((left->type == EXP_INTEGER || left->type == EXP_FLOAT)
				&& (right->type == EXP_INTEGER || right->type == EXP_FLOAT)
			) {
				return 1;
			}
			if (left->type == EXP_BINOP || left->type == EXP_UNOP) {
				return optimize_arith_available(P, left);
			}
			if (right->type == EXP_BINOP || right->type == EXP_UNOP) {
				return optimize_arith_available(P, right);
			}
			return 0;
		}
		case EXP_UNOP: {
			ExpNode* operand = tree->uval->operand;
			if (!optimizable[tree->uval->type]) {
				return 0;
			}
			if (operand->type == EXP_INTEGER || operand->type == EXP_FLOAT) {
				return 1;
			}
			if (operand->type == EXP_BINOP || operand->type == EXP_UNOP) {
				return optimize_arith_available(P, operand);
			}
			return 0;
		}
		default:
			return 0;
	}	
}

/* optimizes a tree to get rid of trivial arithmetic, e.g.
 * 5 + 10 + x can be condensed to 15 + x */
static void
optimize_tree_arith(ParseState* P, ExpNode* tree) {

	ExpNode* new = NULL;
	if (!tree) return;
	switch (tree->type) {
		case EXP_BINOP: {
			ExpNode* left = tree->bval->left;
			ExpNode* right = tree->bval->right;
			/* break if it can't be condensed */
			if (left->type == EXP_BINOP || left->type == EXP_UNOP) {
				optimize_tree_arith(P, left);
			}
			if (right->type == EXP_BINOP || right->type == EXP_UNOP) {
				optimize_tree_arith(P, right);
			}
			if (left->type != EXP_INTEGER && left->type != EXP_FLOAT) {
				break;
			}
			if (right->type != EXP_INTEGER && right->type != EXP_FLOAT) {
				break;
			}	
			if (!optimizable[tree->bval->type]) {
				return;
			}
			/* TODO typecheck */
			new = malloc(sizeof(ExpNode));
			new->type = left->type;
			/* condense integer arithmetic */
			if (new->type == EXP_INTEGER) {
				switch (tree->bval->type) {
					case TOK_PLUS:
						new->ival = left->ival + right->ival;
						break;	
					case TOK_ASTER:
						new->ival = left->ival * right->ival;
						break;
					case TOK_FORSLASH:
						new->ival = left->ival / right->ival;
						break;
					case TOK_HYPHON:
						new->ival = left->ival - right->ival;
						break;
					case TOK_SHL:
						new->ival = left->ival << right->ival;
						break;
					case TOK_SHR:
						new->ival = left->ival >> right->ival;
						break;
					case TOK_GT:
						new->ival = left->ival > right->ival;
						break;
					case TOK_LT:
						new->ival = left->ival < right->ival;
						break;
					case TOK_GE:
						new->ival = left->ival >= right->ival;
						break;
					case TOK_LE:
						new->ival = left->ival <= right->ival;
						break;
					case TOK_EQ:
						new->ival = left->ival == right->ival;
						break;

				}
			/* condense float arithmetic */
			} else {

			}
			break;
		}
		case EXP_UNOP: {
			ExpNode* operand = tree->uval->operand;
			if (operand->type == EXP_BINOP || operand->type == EXP_UNOP) {
				optimize_tree_arith(P, operand);
			}
			if (operand->type != EXP_INTEGER && operand->type != EXP_FLOAT) {
				break;
			}
			if (!optimizable[tree->uval->type]) {
				break;
			}
			
			new = malloc(sizeof(ExpNode));
			new->type = operand->type;
			
			if (operand->type == EXP_INTEGER) {
				switch (tree->uval->type) {
					case TOK_EXCL:
						new->ival = operand->ival == 0;
						break;
				}
			/* floating point arithmetic */
			} else {

			}
			/* no need to create a new node like we do for binary operator
			 * evaluation... just change the operand accordingly */
			 break;
		}
	}
	if (!new) return;
	if (tree->parent) {
		/* remove tree and append new */
		if (tree->parent->type == EXP_BINOP) {
			if (tree->side == LEAF_LEFT) {
				/* LEFT SIDE */
				tree->parent->bval->left = new;
			} else {
				/* RIGHT SIDE */
				tree->parent->bval->right = new;
			}
		} else {
			tree->parent->uval->operand = new;
		}
	} else {
		memcpy(tree, new, sizeof(ExpNode));
	}
}

static ExpNode* 
parse_expression(ParseState* P) {

	if (!P->token || P->token->type == TOK_SEMICOLON) {
		return NULL;
	}

	ExpStack* postfix = calloc(1, sizeof(ExpStack));
	ExpStack* operators = calloc(1, sizeof(ExpStack));

	static const OperatorInfo opinfo[256] = {
		[TOK_COMMA]			= {1, ASSOC_LEFT, OP_BINARY},
		[TOK_ASSIGN]		= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_INCBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_DECBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_MULBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_DIVBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_MODBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_SHLBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_SHRBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_ANDBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_ORBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_XORBY]			= {2, ASSOC_RIGHT, OP_BINARY},
		[TOK_LOGAND]		= {3, ASSOC_LEFT, OP_BINARY},
		[TOK_LOGOR]			= {3, ASSOC_LEFT, OP_BINARY},
		[TOK_EQ]			= {4, ASSOC_LEFT, OP_BINARY},
		[TOK_NOTEQ]			= {4, ASSOC_LEFT, OP_BINARY},
		[TOK_GT]			= {6, ASSOC_LEFT, OP_BINARY},
		[TOK_GE]			= {6, ASSOC_LEFT, OP_BINARY},
		[TOK_LT]			= {6, ASSOC_LEFT, OP_BINARY},
		[TOK_LE]			= {6, ASSOC_LEFT, OP_BINARY},
		[TOK_LINE]			= {7, ASSOC_LEFT, OP_BINARY},
		[TOK_SHL]			= {7, ASSOC_LEFT, OP_BINARY},
		[TOK_SHR]			= {7, ASSOC_LEFT, OP_BINARY},
		[TOK_PLUS]			= {8, ASSOC_LEFT, OP_BINARY},
		[TOK_HYPHON]		= {8, ASSOC_LEFT, OP_BINARY},
		[TOK_ASTER]			= {9, ASSOC_LEFT, OP_BINARY},
		[TOK_PERCENT]		= {9, ASSOC_LEFT, OP_BINARY},
		[TOK_FORSLASH]		= {9, ASSOC_LEFT, OP_BINARY},
		[TOK_AMPERSAND]		= {10, ASSOC_RIGHT, OP_UNARY},
		[TOK_UPCARROT]		= {10, ASSOC_RIGHT, OP_UNARY},
		[TOK_EXCL]			= {10, ASSOC_RIGHT, OP_UNARY},
		[TOK_CAST]			= {10, ASSOC_RIGHT, OP_UNARY},
		[TOK_PERIOD]		= {11, ASSOC_LEFT, OP_BINARY},
		[TOK_INC]			= {11, ASSOC_LEFT, OP_UNARY},
		[TOK_DEC]			= {11, ASSOC_LEFT, OP_UNARY}
	};

	for (; P->token && P->token != P->end_mark; P->token = P->token->next) {
		if (P->token->type == TOK_SEMICOLON) continue;
		/* use assoc to see if it exists */
		if (matches_datatype(P)) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			push->type = EXP_DATATYPE;
			push->tval = parse_datatype(P);	
			if (P->token) {
				P->token = P->token->prev;
			}
			expstack_push(postfix, push);
		} else if (token(P)->type == TOK_CLOSEPAR) {
			ExpNode* top;
			while (1) {
				top = expstack_top(operators);
				if (!top) {
					parse_error(P, "unexpected parenthesis ')'");
					break;
				}
				/* pop and push until an open parenthesis is reached */
				if (top->type == EXP_UNOP && top->uval->type == TOK_OPENPAR) break;
				expstack_push(postfix, expstack_pop(operators));
			}
			expstack_pop(operators);
		} else if (opinfo[P->token->type].assoc || P->token->type == TOK_OPENPAR) {
			int is_cast = 0;
			/* we want to handle a cast and an operator in the exact same way... therefore,
			 * if we see an open parenthesis, we need to ensure that it is a cast.  If it's
			 * not, just push the parenthesis onto the operator stack and continue */
			if (P->token->type == TOK_OPENPAR) {
				P->token = P->token->next;
				if (matches_datatype(P)) {
					is_cast = 1;
				} else {
					/* we reached here if an open parenthesis was found but
					 * it is NOT a cast */
					P->token = P->token->prev;
					ExpNode* push = malloc(sizeof(ExpNode));
					push->type = EXP_UNOP; /* just consider a parenthesis as a unary operator */
					push->uval = malloc(sizeof(UnaryOp));
					push->uval->type = TOK_OPENPAR;
					push->uval->operand = NULL;
					expstack_push(operators, push);
					continue;
				}
			}
			/* NOTE if a cast is found, P->token is the first token in the datatype
			 * (as opposed to the opening parenthesis) */
			ExpNode* top;
			const OperatorInfo* info = is_cast ? &opinfo[TOK_CAST] : &opinfo[P->token->type];
			while (1) {
				top = expstack_top(operators);
				if (!top) break;
				int top_is_unary = top->type == EXP_UNOP;
				int top_is_binary = top->type == EXP_BINOP;
				const OperatorInfo* top_info;
				if (top_is_unary) {
					/* unary operator is on top of stack */
					top_info = &opinfo[top->uval->type];
				} else if (top_is_binary) {
					/* binary operator is on top of stack */
					top_info = &opinfo[top->bval->type];
				} else { 
					/* cast is on top of stack */
					top_info = &opinfo[TOK_CAST];
				}
				/* found open parenthesis, break */
				if (top_is_unary && top->uval->type == TOK_OPENPAR) {
					break;
				}
				if (info->assoc == ASSOC_LEFT) {
					if (info->pres > top_info->pres) break;
				} else {
					if (info->pres >= top_info->pres) break;
				}
				expstack_push(postfix, expstack_pop(operators));
			}
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			if (is_cast) {
				push->type = EXP_CAST;
			} else if (info->optype == OP_UNARY) {
				push->type = EXP_UNOP;
			} else {
				push->type = EXP_BINOP;
			}
			if (push->type == EXP_BINOP) {
				push->bval = malloc(sizeof(BinaryOp));
				push->bval->type = P->token->type;
				push->bval->left = NULL;
				push->bval->right = NULL;
			} else if (push->type == EXP_UNOP) {
				push->uval = malloc(sizeof(UnaryOp));
				push->uval->type = P->token->type;
				push->uval->operand = NULL;
			} else {
				push->cval = malloc(sizeof(TypeCast));
				push->cval->datatype = parse_datatype(P);
				push->cval->operand = NULL;
				make_sure(P, TOK_CLOSEPAR, "expected ')' to close explicit cast");
			}
			expstack_push(operators, push);
		/* if it's a literal, just push it onto the postfix stack */
		} else if (P->token->type == TOK_INT || P->token->type == TOK_FLOAT) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			switch (P->token->type) {
				case TOK_INT:
					push->type = EXP_INTEGER;
					push->ival = strtoll(P->token->word, NULL, 10);
					break;
				case TOK_FLOAT:
					push->type = EXP_FLOAT;
					push->fval = strtod(P->token->word, NULL);
					break;
			}
			expstack_push(postfix, push);
		/* TODO this should probably work with locals
		 * instead of just the identifier */
		} else if (P->token->type == TOK_IDENTIFIER) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->type = EXP_IDENTIFIER;
			push->parent = NULL;
			push->idval = malloc(strlen(P->token->word) + 1);
	   		strcpy(push->idval, P->token->word);
			expstack_push(postfix, push);
		}
	}

	while (expstack_top(operators)) {
		ExpNode* pop = expstack_pop(operators);
		if (pop->type == EXP_UNOP) {
			if (pop->uval->type == TOK_OPENPAR || pop->uval->type == TOK_CLOSEPAR) {
				parse_error(P, "mismatched parenthesis");
			}
		}
		expstack_push(postfix, pop);
	}

	/* now the postfix stack contains the expression
	 * in reverse-polish notation..... generate a tree */

	/* this is the stack we will use to generate the tree...
	 * at the end, only one value should remain on the 
	 * stack.  If there are no values or too many
	 * values, the user entered a malformed expression */
	ExpStack* tree = malloc(sizeof(ExpStack));
	tree->node = NULL;
	tree->next = NULL;
	tree->prev = NULL;

	static const char* malformed = "malformed expression";

	/* iterate through the postfix */
	for (ExpStack* i = postfix; i; i = i->next) {
		ExpNode* node = i->node;
		/* if it's a literal, append to the stack */
		if (node->type == EXP_INTEGER 
			|| node->type == EXP_FLOAT
			|| node->type == EXP_IDENTIFIER
			|| node->type == EXP_DATATYPE
		) {
			expstack_push(tree, node);
		/* if it's a binary operator, form a branch */
		} else if (node->type == EXP_BINOP) {
			/* pop the leaves off of the stack */
			ExpNode* leaf[2];
			for (int j = 0; j < 2; j++) {
				/* TODO error check for malformed expression,
				 * also typecheck in the future */
				leaf[j] = expstack_pop(tree);
				if (!leaf[j]) {
					parse_error(P, malformed);
				}
				leaf[j]->parent = node;
				leaf[j]->side = j == 1 ? LEAF_LEFT : LEAF_RIGHT;
			}
			/* swap order */
			node->bval->left = leaf[1];
			node->bval->right = leaf[0];
			/* throw the branch back onto the stack */
			expstack_push(tree, node);
		} else if (node->type == EXP_UNOP) {
			ExpNode* operand = expstack_pop(tree);
			if (!operand) {
				parse_error(P, malformed);
			}
			operand->parent = node;
			node->uval->operand = operand;
			expstack_push(tree, node);
		} else if (node->type == EXP_CAST) {
			ExpNode* operand = expstack_pop(tree);
			operand->parent = node;
			node->cval->operand = operand;
			expstack_push(tree, node);
		}
	}

	typecheck_expression(P, tree->node);
	
	if (P->options->opt_level >= OPT_ONE) {	
		while (optimize_arith_available(P, tree->node)) {
			optimize_tree_arith(P, tree->node);
		}
	}

	/* there should only be one value in the stack... TODO check this */
	return tree->node;

}

static void
jump_out(ParseState* P) {
	/* on token '}' */
	P->token = P->token->next;
	TreeNode* block = P->current_block;
	if (block == P->root_block) {
		return;
	}
	do {
		/* get rid of current function on jump-out */
		if (block->type == NODE_FUNCTION) {
			P->current_function = NULL;
		}
		block = block->parent;
	} while (block && block->type != NODE_BLOCK);
	if (!block) {
		parse_error(P, "expected '}' before EOF");
	}
	P->current_block = block;
}

/*
 * <typename> ::= <identifier>
 * <datatype> ::= <modifier>* <typename> "^"*
 *
 */
static TreeType*
parse_datatype(ParseState* P) {
	TreeType* type = malloc(sizeof(TreeType));
	type->type_name = NULL;
	type->plevel = 0;
	type->size = 0;
	type->modifier = 0;

	/* find modifiers */	
	int found;
	do {
		found = 0;
		unsigned int mod = get_modifier(P->token->word);
		if (mod == MOD_CFUNC) {
			parse_error(P, "modifier 'cfunc' can only be used in function declarations");
		}
		if (mod) {
			if (type->modifier & mod) {
				parse_error(P, "duplicate modifier '%s' in variable declaration", P->token->word);
			}
			type->modifier |= mod;
			found = 1;
			P->token = P->token->next;
		}
	} while (found);	
	
	/* on typename */
	type->type_name = malloc(strlen(P->token->word) + 1);
	strcpy(type->type_name, P->token->word);
	/* make sure it's a type */
	if (!get_datatype(P, type->type_name)) {
		parse_error(P, "unknown type name '%s'", type->type_name);
	}
	P->token = P->token->next;

	/* on pointer? */
	while (token(P) && P->token->type == TOK_UPCARROT) {
		type->plevel++;
		P->token = P->token->next;
	}
	
	return type;

}

/* expects that the end of the statement is already marked */
static TreeNode*
parse_statement(ParseState* P) {
	TreeNode* node = new_node(P, NODE_STATEMENT);
	node->stateval = parse_expression(P);
	return node;
}

static TreeVariable*
parse_declaration(ParseState* P) {
	/* expects to be on identifier of declaration */
	TreeVariable* decl = malloc(sizeof(TreeVariable));
	decl->identifier = malloc(strlen(P->token->word) + 1);
	strcpy(decl->identifier, P->token->word);
	P->token = P->token->next->next;
	decl->datatype = parse_datatype(P);
	return decl;
}

static void
parse_function(ParseState* P) {
	if (P->current_block != P->root_block) {
		parse_error(P, "functions can only be declared in the main scope");
	}
	/* we're currently on the identifier of the function... scan through
	 * the main block and make sure the funciton hasn't been implemented yet */
	TreeNode* decl = NULL;
	for (TreeNode* i = P->root_block->blockval->child; i; i = i->next) {
		if (i->type != NODE_FUNCTION) continue;
		if (!strcmp(i->funcval->identifier, P->token->word)) {
			if (i->funcval->implemented) {
				parse_error(P, "attempt to re-implement function '%s'", P->token->word);
			} else {
				/* otherwise it's not implemented and we want to copy data to this function */
				decl = i;
				break;
			}
		}
	}
	/* expects to be on the function identifier */
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_FUNCTION;
	node->funcval = malloc(sizeof(TreeFunction));
	node->funcval->identifier = malloc(strlen(P->token->word) + 1);
	node->funcval->params = NULL;
	node->funcval->nparams = 0;
	strcpy(node->funcval->identifier, P->token->word);
	P->token = P->token->next->next;
	/* no need to make sure we're on a semicolon, matches_function() already did that */
	uint32_t mod;
	while ((mod = get_modifier(P->token->word))) {
		node->funcval->modifiers |= mod;
		P->token = P->token->next;
	}
	/* also no need to make sure we're on "(" */
	P->token = P->token->next;
	/* now we're either on an argument list or a ")" */
	while (P->token->type != TOK_CLOSEPAR) {
		node->funcval->nparams++;
		TreeVariable* arg = parse_declaration(P);
		TreeVariableList* list = malloc(sizeof(TreeVariableList));
		list->variable = arg;
		list->next = NULL;
		/* append the arg to list of params */
		if (!node->funcval->params) {
			node->funcval->params = list;
		} else {
			TreeVariableList* i;
			for (i = node->funcval->params; i->next; i = i->next);
			i->next = list;
		}
		if (!P->token) {
			parse_error(P, "unexpected EOF while parsing function argument list");
		}
		if (P->token->type == TOK_CLOSEPAR) {
			P->token = P->token->next;
			break;
		}
		if (P->token->type != TOK_COMMA) {
			parse_error(P, "expected ',' or ')' to follow declaration of argument '%s'", arg->identifier);
		}
		P->token = P->token->next;
	}
	make_sure(P, TOK_ARROW, "expected token '->' to follow function argument list");
	P->token = P->token->next;
	node->funcval->return_type = parse_datatype(P);
	/* if decl isn't NULL, the function was previously declared but
	 * not implemented... so, we want to make sure that the functions
	 * match each other and we want to replace decl in the list of nodes */
	if (decl) {
		TreeVariableList* arg_decl = decl->funcval->params;
		TreeVariableList* arg_impl = node->funcval->params;
		int at_param = 0;
		do {
			at_param++;
			if (!exact_datatype(arg_decl->variable->datatype, arg_impl->variable->datatype)) {
				parse_error(
					P, 
					"implementation of function '%s' doesn't match its declaration... "
					"argument #%d: expected type (%s) but got type (%s)",
					node->funcval->identifier,
					at_param,
					tostring_datatype(arg_decl->variable->datatype),
					tostring_datatype(arg_impl->variable->datatype)
				);
			}
			arg_decl = arg_decl->next;
			arg_impl = arg_impl->next;
		} while (arg_decl && arg_impl);
		/* thank god C has a logical XOR operator, right? */
		if ((!arg_decl && arg_impl) || (arg_decl && !arg_impl)) {
			parse_error(
				P,
				"implementation of function '%s' doesn't have the same number of parameters "
				"its declaration.  Expected %d parameters, got %d",
				node->funcval->identifier,
				decl->funcval->nparams,
				at_param
			);
		}
		if (!exact_datatype(decl->funcval->return_type, node->funcval->return_type)) {
			parse_error(
				P,
				"return type of function '%s' doesn't match its declaration, expected return type "
				"(%s), got (%s)",
				node->funcval->identifier,
				tostring_datatype(decl->funcval->return_type),
				tostring_datatype(node->funcval->return_type)
			);
			parse_error(P, "incorrect return type");
		}

	}
	/* if we're on a semicolon, it's a function declaration, not implementation */
	node->funcval->implemented = P->token->type != TOK_SEMICOLON;
	if (!node->funcval->implemented && decl) {
		parse_error(P, "attempt to re-declare function '%s'", node->funcval->identifier);
	}
	if (!node->funcval->implemented) {
		node->funcval->child = NULL;
		P->token = P->token->next;
		append(P, node);
		return;
	}
	P->current_function = node;
	/* it is valid to declare a function like you would a math function, e.g.
	 * square: (n: int) -> int = n * n;
	 */
	append(P, node);
	/* now we're free to remove decl from the list */
	if (decl->prev) {
		decl->prev->next = decl->next;
	} else {
		P->root_block->blockval->child = decl->next;
	}
	free(decl);
	if (P->token->type == TOK_ASSIGN) {
		/* check if it's a 'short' function */
		P->token = P->token->next;
		TreeNode* ret = malloc(sizeof(TreeNode));
		ret->type = NODE_RETURN;
		mark_expression(P, TOK_NULL, TOK_SEMICOLON);
		ret->stateval = parse_expression(P);
		P->token = P->token->next;
		append(P, ret);
		/* set current function back to NULL because were
		 * already finished parsing this function body */
		P->current_function = NULL;
	}
}

static void
parse_return(ParseState* P) {
	/* no need to check if we're inside of a function... append does that */
	/* starts on token RETURN */
	P->token = P->token->next;
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_RETURN;
	mark_expression(P, TOK_NULL, TOK_SEMICOLON);
	node->stateval = parse_expression(P);
	P->token = P->token->next;
	append(P, node);
}

static void
parse_break(ParseState* P) {
	/* starts on token BREAK */
	P->token = P->token->next;
	TreeNode* node = malloc(sizeof(TreeNode));
	make_sure(P, TOK_SEMICOLON, "expected ';' after token 'break'");
	node->type = NODE_BREAK;
	P->token = P->token->next;
	append(P, node);
}

static void
parse_continue(ParseState* P) {
	/* starts on token BREAK */
	P->token = P->token->next;
	TreeNode* node = malloc(sizeof(TreeNode));
	make_sure(P, TOK_SEMICOLON, "expected ';' after token 'continue'");
	node->type = NODE_CONTINUE;
	P->token = P->token->next;
	append(P, node);
}

static void 
parse_if(ParseState* P) {
	/* starts on token IF */
	P->token = P->token->next;
	make_sure(P, TOK_OPENPAR, "expected '(' to begin if condition");
	P->token = P->token->next;
	TreeNode* node = malloc(sizeof(TreeNode));
	node->ifval = malloc(sizeof(TreeIf));
	node->type = NODE_IF;
	/* mark the end of the condition */
	mark_expression(P, TOK_OPENPAR, TOK_CLOSEPAR); 
	/* parse the condition */
	node->ifval->condition = parse_expression(P);
	node->ifval->child = NULL;
	P->token = P->token->next;
	append(P, node);
}

static void 
parse_while(ParseState* P) {
	/* starts on token WHILE */
	P->token = P->token->next;
	make_sure(P, TOK_OPENPAR, "expected '(' to begin while condition");
	P->token = P->token->next;
	TreeNode* node = malloc(sizeof(TreeNode));
	node->whileval = malloc(sizeof(TreeWhile));
	node->type = NODE_WHILE;
	/* mark the end of the condition */
	mark_expression(P, TOK_OPENPAR, TOK_CLOSEPAR); 
	/* parse the condition */
	node->whileval->condition = parse_expression(P);
	node->whileval->child = NULL;
	P->token = P->token->next;
	P->current_loop = node;
	append(P, node);
}

static void
parse_for(ParseState* P) {
	/* starts on token FOR */
	P->token = P->token->next;
	make_sure(P, TOK_OPENPAR, "expected '(' after token 'for'");
	P->token = P->token->next;
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_FOR;
	node->forval = malloc(sizeof(TreeFor));
	node->forval->child = NULL;
	/* initializer ends with a semicolon */
	mark_expression(P, TOK_NULL, TOK_SEMICOLON);
	node->forval->initializer = parse_expression(P);
	P->token = P->token->next;
	/* condition ends with a semicolon */
	mark_expression(P, TOK_NULL, TOK_SEMICOLON);
	node->forval->condition = parse_expression(P);
	P->token = P->token->next;
	/* statements ends with a closing parenthesis */
	mark_expression(P, TOK_OPENPAR, TOK_CLOSEPAR);
	node->forval->statement = parse_expression(P);	
	P->token = P->token->next;
	P->current_loop = node;
	append(P, node);
}

/* this function is unlike other parse functions... It doesn't
 * actually parse the block, it just sets the block up for
 * nodes to be placed inside of it */
static void
parse_block(ParseState* P) {
	/* starts on token '{' */
	P->token = P->token->next;
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_BLOCK;
	node->blockval = malloc(sizeof(TreeBlock));
	node->blockval->locals = NULL;
	node->blockval->child = NULL;
	append(P, node);	
}

ParseState*
generate_tree(LexState* L, ParseOptions* options) {
	ParseState* P = malloc(sizeof(ParseState));
	P->filename = L->filename;
	P->total_lines = L->total_lines;
	P->options = options;
	P->token = L->tokens;
	P->end_mark = NULL;	
	P->current_function = NULL;
	P->current_loop = NULL;
	P->to_append = NULL;
	P->defined_types = malloc(sizeof(TreeTypeList));
	P->defined_types->next = NULL;
	P->defined_types->prev = NULL;
	P->defined_types->datatype = NULL;
	
	/* establish primitive int */
	TreeType* type_int = malloc(sizeof(TreeType));
	type_int->type_name = "int";
	type_int->plevel = 0;
	type_int->size = 8;
	type_int->modifier = 0;
	P->type_integer = type_int;
	register_datatype(P, type_int);
	
	/* establish primitive float */
	TreeType* type_float = malloc(sizeof(TreeType));
	type_float->type_name = "float";
	type_float->plevel = 0;
	type_float->size = 8;
	type_float->modifier = 0;
	P->type_float = type_float;
	register_datatype(P, type_float);
	
	/* establish primitive byte */
	TreeType* type_byte = malloc(sizeof(TreeType));
	type_byte->type_name = "byte";
	type_byte->plevel = 0;
	type_byte->size = 1;
	type_byte->modifier = 0;
	P->type_byte = type_byte;
	register_datatype(P, type_byte);

	/* establish void type */
	TreeType* type_void = malloc(sizeof(TreeType));
	type_void->type_name = "void";
	type_void->plevel = 0;
	type_void->size = 0;
	type_void->modifier = 0;
	P->type_void = type_void;
	register_datatype(P, type_void);

	/* establish the root block */
	TreeNode* root = malloc(sizeof(TreeNode));
	root->type = NODE_BLOCK;
	root->parent = NULL;
	root->next = NULL;
	root->prev = NULL;
	root->blockval = malloc(sizeof(TreeBlock));
	root->blockval->child = NULL;
	root->blockval->locals = NULL;
	P->root_block = root;
	P->current_block = root;
	
	while (P->token) {	
		TreeNode* node;
		if (P->token->type == TOK_SEMICOLON) {
			P->token = P->token->next;
			continue;
		} else if (matches_function(P)) {
			parse_function(P);
			continue;
		}
		switch (P->token->type) {
			case TOK_IF:
				parse_if(P);
				break;
			case TOK_WHILE:
				parse_while(P);
				break;
			case TOK_FOR:
				parse_for(P);
				break;
			case TOK_RETURN:
				parse_return(P);
				break;
			case TOK_BREAK:
				parse_break(P);
				break;
			case TOK_CONTINUE:
				parse_continue(P);
				break;
			case TOK_OPENCURL:
				parse_block(P);
				break;
			case TOK_CLOSECURL:
				if (P->current_block == P->root_block) {
					parse_error(P, "token '}' doesn't close anything");
				}
				jump_out(P);
				break;
			default:
				if (peek(P, 1) && peek(P, 1)->type == TOK_COLON) {
					TreeVariable* decl = parse_declaration(P);	
					make_sure(
						P,
						TOK_SEMICOLON,
						"expected ';' after declaration of '%s', got token '%s'",
						decl->identifier,
						P->token->word
					);
					P->token = P->token->next;
					register_local(P, decl);
				} else {
					/* expect an expression ending with a semicolon */
					mark_expression(P, TOK_NULL, TOK_SEMICOLON);
					node = parse_statement(P);
					append(P, node);
				}
				break;
		}
				
	}

	if (P->options->opt_level >= OPT_TWO) {
		optimize_branching(P, P->root_block);	
	}

	print_node(P->root_block, 0);

	return P;
}	
