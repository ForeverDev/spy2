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
static int simplify_available(ParseState*, ExpNode*);
static void print_expression_tree(ExpNode*, int);
static Token* token(ParseState*);
static int on(ParseState*, int, ...);
static void next(ParseState*, int);
static void register_datatype(ParseState*, SpyType*);
static SpyType* get_datatype(ParseState*, const char*);
static int matches_datatype(ParseState*);
static void make_sure(ParseState*, TokenType, const char*);
static void print_datatype(SpyType*);
static void mark_expression(ParseState*, TokenType, TokenType);

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
	/* resort to last line if there is no token */
	printf("\n\tline: %d\n\n", P->token ? P->token->line : P->total_lines);
	va_end(list);
	exit(1);
}

static void
make_sure(ParseState* P, TokenType type, const char* err) {
	if (!P->token || P->token->type != type) {
		parse_error(P, err);
	}
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
			case EXP_BINOP:
				printf("%c", tt_to_word(node->bval->type)); 
				break;
			case EXP_UNOP:
				printf("%c", tt_to_word(node->uval->type)); 
				break;
			case EXP_STRING:
				printf("%s", node->sval);
				break;
			case EXP_INTEGER:
				printf("%lld", node->ival);
				break;
			case EXP_FLOAT:
				printf("%lf", node->fval);
				break;
			case EXP_IDENTIFIER:
				printf("%s", node->idval);
				break;
			case EXP_CAST:
				print_datatype(node->cval->datatype);
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
mark_expression(ParseState* P, TokenType inc, TokenType dec) {
	int count = 1;
	Token* search = P->token;
	for (Token* i = P->token; i; i = i->next) {
		if (i->type == inc) {
			count++;
		} else if (i->type == dec) {
			count--;
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

#define INDENT(n) for (int _=0; _<(n); _++) printf("\t")

static void
print_expression_tree(ExpNode* tree, int indent) {
	INDENT(indent);
	switch (tree->type) {
		case EXP_BINOP:
			printf("%c\n", tt_to_word(tree->bval->type));
			print_expression_tree(tree->bval->left, indent + 1);	
			print_expression_tree(tree->bval->right, indent + 1);	
			break;
		case EXP_UNOP:
			printf("%c\n", tt_to_word(tree->uval->type));
			print_expression_tree(tree->uval->operand, indent + 1);	
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
			print_datatype(tree->cval->datatype);
			printf("\n");
			print_expression_tree(tree->cval->operand, indent + 1);
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

static int
simplify_available(ParseState* P, ExpNode* tree) {
	switch (tree->type) {
		case EXP_BINOP: {
			ExpNode* left = tree->bval->left;
			ExpNode* right = tree->bval->right;
			if ((left->type == EXP_INTEGER || left->type == EXP_FLOAT)
				&& (right->type == EXP_INTEGER || right->type == EXP_FLOAT)
			) {
				return 1;
			}
			if (left->type == EXP_BINOP || left->type == EXP_UNOP) {
				return simplify_available(P, left);
			}
			if (right->type == EXP_BINOP || right->type == EXP_UNOP) {
				return simplify_available(P, right);
			}
			return 0;
		}
		case EXP_UNOP: {
			ExpNode* operand = tree->uval->operand;
			if (operand->type == EXP_INTEGER || operand->type == EXP_FLOAT) {
				return 1;
			}
			if (operand->type == EXP_BINOP || operand->type == EXP_UNOP) {
				return simplify_available(P, operand);
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
simplify_tree(ParseState* P, ExpNode* tree) {
	ExpNode* new = NULL;
	switch (tree->type) {
		case EXP_BINOP: {
			ExpNode* left = tree->bval->left;
			ExpNode* right = tree->bval->right;
			/* break if it can't be condensed */
			if (left->type == EXP_BINOP || left->type == EXP_UNOP) {
				simplify_tree(P, left);
			}
			if (right->type == EXP_BINOP || right->type == EXP_UNOP) {
				simplify_tree(P, right);
			}
			if (left->type != EXP_INTEGER && left->type != EXP_FLOAT) {
				break;
			}
			if (right->type != EXP_INTEGER && right->type != EXP_FLOAT) {
				break;
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

				}
			/* condense float arithmetic */
			} else {

			}
			break;
		}
		case EXP_UNOP: {
			ExpNode* operand = tree->uval->operand;
			if (operand->type == EXP_BINOP || operand->type == EXP_UNOP) {
				simplify_tree(P, operand);
			}
			if (operand->type != EXP_INTEGER && operand->type != EXP_FLOAT) {
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
		[TOK_CAST]			= {10, ASSOC_RIGHT, OP_UNARY},
		[TOK_PERIOD]		= {11, ASSOC_LEFT, OP_BINARY}
	};

	for (; P->token && P->token != P->end_mark; P->token = P->token->next) {
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
		} else if (node->type == EXP_UNOP) {
			ExpNode* operand = expstack_pop(tree);
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
	
	while (simplify_available(P, tree->node)) {
		simplify_tree(P, tree->node);
	}

	/* there should only be one value in the stack... TODO check this */
	return tree->node;

}

ParseState*
generate_tree(LexState* L) {
	ParseState* P = malloc(sizeof(ParseState));
	P->filename = L->filename;
	P->total_lines = L->total_lines;
	P->token = L->tokens;
	P->end_mark = NULL;	
	P->defined_types = malloc(sizeof(SpyTypeList));
	P->defined_types->next = NULL;
	P->defined_types->prev = NULL;
	P->defined_types->datatype = NULL;
	
	/* establish primitive int */
	SpyType* type_int = malloc(sizeof(SpyType));
	type_int->type_name = "int";
	type_int->plevel = 0;
	type_int->size = 8;
	type_int->modifier = 0;
	register_datatype(P, type_int);
	
	/* establish primitive float */
	SpyType* type_float = malloc(sizeof(SpyType));
	type_float->type_name = "float";
	type_float->plevel = 0;
	type_float->size = 8;
	type_float->modifier = 0;
	register_datatype(P, type_float);
	
	/* establish primitive byte */
	SpyType* type_byte = malloc(sizeof(SpyType));
	type_byte->type_name = "byte";
	type_byte->plevel = 0;
	type_byte->size = 1;
	type_byte->modifier = 0;
	register_datatype(P, type_byte);
	
	while (P->token) {	
		TreeNode* node;
		switch (P->token->type) {
			case TOK_IF:
				//node = parse_if(P);
				break;
			default:
				//node = parse_statement(P);
				break;
		}
				
	}

	return P;
}	
