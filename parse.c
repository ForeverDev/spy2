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

typedef struct ExpStack ExpStack;
typedef struct OperatorInfo OperatorInfo;

static SpyType* parse_datatype(ParseState*);
static ExpNode* expression_to_tree(ParseState*);
static void simplify_tree(ParseState*, ExpNode*);
static void print_expression_tree(ExpNode*, int);
static Token* token(ParseState*);
static int on(ParseState*, int, ...);
static void next(ParseState*, int);
static void register_datatype(ParseState*, SpyType*);
static SpyType* get_datatype(ParseState*, const char*);
static int matches_datatype(ParseState*);

static void expstack_push(ExpStack*, ExpNode*);
static void expstack_print(ExpStack*);
static ExpNode* expstack_pop(ExpStack*);
static ExpNode* expstack_top(ExpStack*);

static void parse_error(ParseState*, const char*, ...); 

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

/* only used to generate a postfix representation
 * of an expression */
struct ExpStack {
	ExpNode* node;
	ExpStack* next;
	ExpStack* prev;
};

static const char* modifiers[] = {
	"static", "const", "volatile"
};

static void
parse_error(ParseState* P, const char* format, ...) {
	va_list list;
	va_start(list, format);
	printf("\n\n***SPYRE COMPILE-TIME ERROR***\n\n");
	printf("\tmessage: ");
	vprintf(format, list);
	printf("\n\tline: %d\n\n", P->token->line);
	va_end(list);
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

static void
expstack_print(ExpStack* stack) {
	for (ExpStack* i = stack; i; i = i->next) {
		ExpNode* node = i->node;
		if (!node) break;
		switch (node->type) {
			case NODE_BINOP:
				printf("%c", tt_to_word(node->bval->type)); 
				break;
			case NODE_UNOP:
				printf("%c", tt_to_word(node->uval->type)); 
				break;
			case NODE_STRING:
				printf("%s", node->sval);
				break;
			case NODE_INTEGER:
				printf("%lld", node->ival);
				break;
			case NODE_FLOAT:
				printf("%lf", node->fval);
				break;
			case NODE_IDENTIFIER:
				printf("%s", node->idval);
				break;
			default:
				break;
		}
		printf(" ");
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
register_datatype(ParseState* P, SpyType* type) {
	if (!P->defined_types->datatype) {
		P->defined_types->datatype = type;
		return;
	}
	SpyTypeList* list;
	for (list = P->defined_types; list->next; list = list->next);
	SpyTypeList* new = malloc(sizeof(SpyTypeList));
	new->datatype = type;
	new->next = NULL;
	new->prev = list;
	list->next = new;
}

static SpyType*
get_datatype(ParseState* P, const char* type_name) {
	for (SpyTypeList* i = P->defined_types; i && i->datatype; i = i->next) {
		if (!strcmp(i->datatype->type_name, type_name)) {
			return i->datatype;
		}
	}
	return NULL;
}

static void
print_datatype(SpyType* type) {
	printf("(");
	for (int i = 0; i < 3; i++) {
		if ((type->modifier >> i) & 0x1) {
			printf("%s ", modifiers[i]);
		}
	}
	printf("%s", type->type_name);
	for (int i = 0; i < type->plevel; i++) {
		printf("^");	
	}
	printf(")");
}

#define INDENT(n) for (int _=0; _<(n); _++) printf("   ")

static void
print_expression_tree(ExpNode* tree, int indent) {
	INDENT(indent);
	switch (tree->type) {
		case NODE_BINOP:
			printf("%c\n", tt_to_word(tree->bval->type));
			print_expression_tree(tree->bval->left, indent + 1);	
			print_expression_tree(tree->bval->right, indent + 1);	
			break;
		case NODE_UNOP:
			printf("%c\n", tt_to_word(tree->uval->type));
			print_expression_tree(tree->uval->operand, indent + 1);	
			break;
		case NODE_INTEGER:
			printf("%lld\n", tree->ival);
			break;
		case NODE_FLOAT:
			printf("%lf\n", tree->fval);
			break;
		case NODE_IDENTIFIER:
			printf("%s\n", tree->idval);
			break;
		case NODE_DATATYPE:
			print_datatype(tree->tval);
			printf("\n");
			break;
	}
}

/* tells whether or not the parser is looking at a datatype */
static int
matches_datatype(ParseState* P) {
	for (int i = 0; i < 3; i++) {
		if (!strcmp(modifiers[i], P->token->word)) {
			return 1;
		}
	}
	return get_datatype(P, P->token->word) != NULL;
}

/*
 * <typename> ::= <identifier>
 * <datatype> ::= <modifier>* <typename> "^"*
 *
 */
static SpyType*
parse_datatype(ParseState* P) {
	SpyType* type = malloc(sizeof(SpyType));
	type->type_name = NULL;
	type->plevel = 0;
	type->size = 0;
	type->modifier = 0;

	/* find modifiers */	
	int found;
	do {
		found = 0;
		for (int i = 0; i < 3; i++) {
			if (!strcmp(P->token->word, modifiers[i])) {
				type->modifier |= (
					i == 0 ? MOD_STATIC :
					i == 1 ? MOD_CONST : MOD_VOLATILE
				);
				found = 1;
				P->token = P->token->next;
				break;
			}		
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

/* optimizes a tree to get rid of trivial arithmetic, e.g.
 * 5 + 10 + x can be condensed to 15 + x */
static void
simplify_tree(ParseState* P, ExpNode* tree) {
	ExpNode* new = NULL;
	switch (tree->type) {
		case NODE_BINOP: {
			ExpNode* left = tree->bval->left;
			ExpNode* right = tree->bval->right;
			/* break if it can't be condensed */
			if (left->type == NODE_BINOP || left->type == NODE_UNOP) {
				simplify_tree(P, left);
			}
			if (right->type == NODE_BINOP || right->type == NODE_UNOP) {
				simplify_tree(P, right);
			}
			if (left->type != NODE_INTEGER && left->type != NODE_FLOAT) {
				break;
			}
			if (right->type != NODE_INTEGER && right->type != NODE_FLOAT) {
				break;
			}	
			/* TODO typecheck */
			new = malloc(sizeof(ExpNode));
			new->type = left->type;
			/* condense integer arithmetic */
			if (new->type == NODE_INTEGER) {
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

				}
			/* condense float arithmetic */
			} else {

			}
			break;
		}
		case NODE_UNOP: {
			ExpNode* operand = tree->uval->operand;
			if (operand->type == NODE_BINOP || operand->type == NODE_UNOP) {
				simplify_tree(P, operand);
			}
			if (operand->type != NODE_INTEGER && operand->type != NODE_FLOAT) {
				break;
			}
			
			new = malloc(sizeof(ExpNode));
			new->type = operand->type;
			
			if (operand->type == NODE_INTEGER) {
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
		if (tree->parent->type == NODE_BINOP) {
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
		//new->parent = tree;
	} else {
		memcpy(tree, new, sizeof(ExpNode));
	}
}

static ExpNode* 
expression_to_tree(ParseState* P) {
	ExpStack* postfix = calloc(1, sizeof(ExpStack));
	ExpStack* operators = calloc(1, sizeof(ExpStack));

	static const OperatorInfo opinfo[256] = {
		[TOK_COMMA]			= {1, ASSOC_LEFT, OP_BINARY},
		[TOK_ASSIGN]		= {2, ASSOC_RIGHT, OP_BINARY},
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
		[TOK_PERIOD]		= {11, ASSOC_LEFT, OP_BINARY}
	};

	for (; token(P); next(P, 1)) {
		/* use assoc to see if it exists */
		if (matches_datatype(P)) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			push->type = NODE_DATATYPE;
			push->tval = parse_datatype(P);	
			if (P->token) {
				P->token = P->token->prev;
			}
			expstack_push(postfix, push);
		} else if (token(P)->type == TOK_OPENPAR) {
			P->token = P->token->next;
			if (matches_datatype(P)) {
				/* check if cast */
			} else {
				P->token = P->token->prev;
			}
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			push->type = NODE_UNOP; /* just consider an open parenthesis as a unary operator */
			push->uval = malloc(sizeof(UnaryOp));
			push->uval->type = TOK_OPENPAR;
			push->uval->operand = NULL; /* no operand for parenthesis */
			expstack_push(operators, push);
		} else if (token(P)->type == TOK_CLOSEPAR) {
			ExpNode* top;
			while (1) {
				top = expstack_top(operators);
				if (!top) break;
				/* pop and push until an open parenthesis is reached */
				if (top->type == NODE_UNOP && top->uval->type == TOK_OPENPAR) break;
				expstack_push(postfix, expstack_pop(operators));
			}
			expstack_pop(operators);
		} else if (opinfo[P->token->type].assoc) {
			ExpNode* top;
			const OperatorInfo* info = &opinfo[P->token->type];
			while (1) {
				top = expstack_top(operators);
				if (!top) break;
				int is_unary = top->type == NODE_UNOP;
				int is_binary = !is_unary;
				const OperatorInfo* top_info;
				if (top->type == NODE_UNOP) {
					top_info = &opinfo[top->uval->type];
				} else {
					top_info = &opinfo[top->bval->type];
				}
				/* found open parenthesis, break */
				if (is_unary && top->uval->type == TOK_OPENPAR) {
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
			push->type = opinfo[P->token->type].optype == OP_UNARY ? NODE_UNOP : NODE_BINOP;
			if (push->type == NODE_BINOP) {
				push->bval = malloc(sizeof(BinaryOp));
				push->bval->type = P->token->type;
				push->bval->left = NULL;
				push->bval->right = NULL;
			} else {
				push->uval = malloc(sizeof(UnaryOp));
				push->uval->type = P->token->type;
				push->uval->operand = NULL;
			}
			expstack_push(operators, push);
		} else if (
			P->token->type == TOK_INT ||
			P->token->type == TOK_FLOAT
		) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			switch (P->token->type) {
				case TOK_INT:
					push->type = NODE_INTEGER;
					push->ival = strtoll(P->token->word, NULL, 10);
					break;
				case TOK_FLOAT:
					push->type = NODE_FLOAT;
					push->fval = strtod(P->token->word, NULL);
					break;
			}
			expstack_push(postfix, push);
		/* TODO this should probably work with locals
		 * instead of just the identifier */
		} else if (P->token->type == TOK_IDENTIFIER) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->type = NODE_IDENTIFIER;
			push->parent = NULL;
			push->idval = malloc(strlen(P->token->word) + 1);
	   		strcpy(push->idval, P->token->word);
			expstack_push(postfix, push);
		}
	}

	while (expstack_top(operators)) {
		expstack_push(postfix, expstack_pop(operators));
	}

	expstack_print(postfix);

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
	
	/* iterate through the postfix */
	for (ExpStack* i = postfix; i; i = i->next) {
		ExpNode* node = i->node;
		/* if it's a literal, append to the stack */
		if (node->type == NODE_INTEGER 
			|| node->type == NODE_FLOAT
			|| node->type == NODE_IDENTIFIER
			|| node->type == NODE_DATATYPE
		) {
			expstack_push(tree, node);
		/* if it's a binary operator, form a branch */
		} else if (node->type == NODE_BINOP) {
			/* pop the leaves off of the stack */
			ExpNode* leaf[2];
			for (int i = 0; i < 2; i++) {
				/* TODO error check for malformed expression,
				 * also typecheck in the future */
				leaf[i] = expstack_pop(tree);
				leaf[i]->parent = node;
				leaf[i]->side = i == 1 ? LEAF_LEFT : LEAF_RIGHT;
			}
			/* swap order */
			node->bval->left = leaf[1];
			node->bval->right = leaf[0];
			/* throw the branch back onto the stack */
			expstack_push(tree, node);
		} else if (node->type == NODE_UNOP) {
			ExpNode* operand = expstack_pop(tree);
			operand->parent = node;
			node->uval->operand = operand;
			expstack_push(tree, node);
		}
	}
	
	/* TODO
	 * this is bad, make it so it only loops until
	 * no further simplifications need to be made */	
	for (int i = 0; i < 10; i++) {	
		simplify_tree(P, tree->node);
	}

	print_expression_tree(tree->node, 0);
	
	/* there should only be one value in the stack... TODO check this */
	return tree->node;

}

ParseState*
generate_tree(Token* tokens) {
	ParseState* P = malloc(sizeof(ParseState));
	P->token = tokens;
	P->defined_types = malloc(sizeof(SpyTypeList));
	P->defined_types->next = NULL;
	P->defined_types->prev = NULL;
	P->defined_types->datatype = NULL;
	
	/* establish primitive types */
	SpyType* type_int = malloc(sizeof(SpyType));
	type_int->type_name = "int";
	type_int->plevel = 0;
	type_int->size = 8;
	type_int->modifier = 0;

	SpyType* type_float = malloc(sizeof(SpyType));
	type_float->type_name = "float";
	type_float->plevel = 0;
	type_float->size = 8;
	type_float->modifier = 0;

	SpyType* type_byte = malloc(sizeof(SpyType));
	type_byte->type_name = "byte";
	type_byte->plevel = 0;
	type_byte->size = 1;
	type_byte->modifier = 0;
	
	register_datatype(P, type_int);
	register_datatype(P, type_float);
	register_datatype(P, type_byte);
	
	while (token(P)) {
		expression_to_tree(P);
	}

	return P;
}	
