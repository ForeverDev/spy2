#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parse.h"

typedef struct ExpStack ExpStack;
typedef struct OperatorInfo OperatorInfo;

static ExpNode* expression_to_tree(ParseState*);
static void print_expression_tree(ExpNode*, int);
static Token* token(ParseState*);
static int on(ParseState*, int, ...);
static void next(ParseState*, int);

static void expstack_push(ExpStack*, ExpNode*);
static void expstack_print(ExpStack*);
static ExpNode* expstack_pop(ExpStack*);
static ExpNode* expstack_top(ExpStack*);

struct OperatorInfo {
	unsigned int pres;
	enum OperatorAssoc {
		ASSOC_LEFT = 1,
		ASSOC_RIGHT = 2
	} assoc;
};

/* only used to generate a postfix representation
 * of an expression */
struct ExpStack {
	ExpNode* node;
	ExpStack* next;
	ExpStack* prev;
};

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
				printf("%c", tt_to_word(node->bval.type)); 
				break;
			case NODE_UNOP:
				printf("%c", tt_to_word(node->uval.type)); 
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

static ExpNode* 
expression_to_tree(ParseState* P) {
	ExpStack* postfix = calloc(1, sizeof(ExpStack));
	ExpStack* operators = calloc(1, sizeof(ExpStack));

	static const OperatorInfo opinfo[256] = {
		[TOK_COMMA]			= {1, ASSOC_LEFT},
		[TOK_ASSIGN]		= {2, ASSOC_RIGHT},
		[TOK_LOGAND]		= {3, ASSOC_LEFT},
		[TOK_LOGOR]			= {3, ASSOC_LEFT},
		[TOK_EQ]			= {4, ASSOC_LEFT},
		[TOK_NOTEQ]			= {4, ASSOC_LEFT},
		[TOK_GT]			= {6, ASSOC_LEFT},
		[TOK_GE]			= {6, ASSOC_LEFT},
		[TOK_LT]			= {6, ASSOC_LEFT},
		[TOK_LE]			= {6, ASSOC_LEFT},
		[TOK_LINE]			= {7, ASSOC_LEFT},
		[TOK_SHL]			= {7, ASSOC_LEFT},
		[TOK_SHR]			= {7, ASSOC_LEFT},
		[TOK_PLUS]			= {8, ASSOC_LEFT},
		[TOK_HYPHON]		= {8, ASSOC_LEFT},
		[TOK_ASTER]			= {9, ASSOC_LEFT},
		[TOK_PERCENT]		= {9, ASSOC_LEFT},
		[TOK_FORSLASH]		= {9, ASSOC_LEFT},
		[TOK_AMPERSAND]		= {10, ASSOC_RIGHT},
		[TOK_UPCARROT]		= {10, ASSOC_RIGHT},
		[TOK_EXCL]			= {10, ASSOC_RIGHT},
		[TOK_PERIOD]		= {11, ASSOC_LEFT}
	};

	for (; token(P); next(P, 1)) {
		/* use assoc to see if it exists */
		if (token(P)->type == TOK_OPENPAR) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			push->type = NODE_UNOP; /* just consider an open parenthesis as a unary operator */
			push->uval.type = TOK_OPENPAR;
			expstack_push(operators, push);
		} else if (token(P)->type == TOK_CLOSEPAR) {
			ExpNode* top;
			while (1) {
				top = expstack_top(operators);
				if (!top) break;
				/* pop and push until an open parenthesis is reached */
				if (top->type == NODE_UNOP && top->uval.type == TOK_OPENPAR) break;
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
					top_info = &opinfo[top->uval.type];
				} else {
					top_info = &opinfo[top->bval.type];
				}
				/* found open parenthesis, break */
				if (is_unary && top->uval.type == TOK_OPENPAR) {
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
			push->type = NODE_BINOP;
			push->bval.type = P->token->type;
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
			}
			/* swap order */
			node->bval.left = leaf[1];
			node->bval.right = leaf[0];
			/* throw the branch back onto the stack */
			expstack_push(tree, node);
		}
	}

	print_expression_tree(tree->node, 0);
	
	/* there should only be one value in the stack... TODO check this */
	return tree->node;

}

#define INDENT(n) for (int _=0; _<(n); _++) printf("   ")

static void
print_expression_tree(ExpNode* tree, int indent) {
	INDENT(indent);
	switch (tree->type) {
		case NODE_BINOP:
			printf("%c\n", tt_to_word(tree->bval.type));
			print_expression_tree(tree->bval.left, indent + 1);	
			print_expression_tree(tree->bval.right, indent + 1);	
			break;
		case NODE_UNOP:
			printf("%c\n", tt_to_word(tree->uval.type));
			print_expression_tree(tree->uval.operand, indent + 1);	
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
	}
}

ParseState*
generate_tree(Token* tokens) {
	ParseState* P = malloc(sizeof(ParseState));
	P->token = tokens;
	
	while (token(P)) {
		expression_to_tree(P);
		switch (P->token->type) {
			
		}
	}

	return P;
}	
