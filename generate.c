#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "generate.h"

#define FORMAT_FUNCTION "__FUNC__%s"
#define FORMAT_LABEL "__LABEL__%04d"

#define FORMAT_FUNCTION_HEAD FORMAT_FUNCTION ":\n"
#define FORMAT_LABEL_HEAD FORMAT_LABEL ":\n"

typedef struct VMInstruction VMInstruction;

struct VMInstruction {
	int has_prefix;
	const char* name; /* mneumonic but I don't want to remember how to spell it.. */
};

static const VMInstruction arith_instructions[] = {
	[TOK_PLUS] = {1, "add"},
	[TOK_HYPHON] = {1, "sub"},
	[TOK_ASTER] = {1, "mul"},
	[TOK_FORSLASH] = {1, "div"}
};

/* writer functions */
static void outb(CompileState*, const char*, ...);
static void pushb(CompileState*, const char*, ...);
static void popb(CompileState*);

/* generate functions */
static void generate_if(CompileState*);
static void generate_function(CompileState*);
static void generate_expression(CompileState*, ExpNode*);

/* misc function */
static int advance(CompileState*);
static TreeNode* get_child(TreeNode*);

/* writes to the output file */
static void 
outb(CompileState* C, const char* format, ...) {
	va_list list;
	va_start(list, format);
	vfprintf(C->handle, format, list);
	va_end(list);
}

/* writes to the instruction stack */
static void
pushb(CompileState* C, const char* format, ...) {

	/* setup format list */
	va_list arg_list;
	va_start(arg_list, format);

	/* get a pointer to the last object on the stack... we are
	 * either going to append a literal to it, or append a whole
	 * new object to the stack */
	InstructionStack* list = C->ins_stack;
	while (list && list->next) {
		list = list->next;
	}
	/* regardless of what happens, we will need a LiteralList */
	LiteralList* lit_list = malloc(sizeof(LiteralList));
	lit_list->next = NULL;
	lit_list->literal = malloc(512); /* 512 bytes is EASILY enough */
	vsprintf(lit_list->literal, format, arg_list);
	if (!list || list->correspond != C->at) {
		/* ... append a whole new object ... */
		InstructionStack* new = malloc(sizeof(InstructionStack));
		new->correspond = C->at;
		new->instructions = lit_list;
		new->next = NULL;
		new->prev = NULL;
		if (C->ins_stack) {
			/* if a stack exists, just append */
			list->next = new;
			new->prev = list;
		} else {
			/* otherwise just assign to stack */
			C->ins_stack = new;
		}
	} else {
		/* just append a new instruction */
		LiteralList* lit_tail = list->instructions;
		while (lit_tail->next) {
			lit_tail = lit_tail->next;
		}
		lit_tail->next = lit_list;
	}

	va_end(arg_list);
}

static void
popb(CompileState* C) {
	if (!C->ins_stack) {
		return;
	}
	InstructionStack* tail = C->ins_stack;
	while (tail->next) {
		tail = tail->next;
	}
	
	/* detach the tail, (pop it off) */
	if (tail->prev) {
		tail->prev->next = NULL;
	} else {
		C->ins_stack = NULL;
	}

	/* now write the instructions */
	LiteralList* i = tail->instructions;
	while (i) {
		/* write the instruction */
		outb(C, i->literal);	

		/* cleanup and move to next */	
		LiteralList* to_free = i;
		i = i->next;
		free(to_free->literal);
		free(to_free);
	}

	free(tail);
}

/* returns the child of a node... note that if the node does not have
 * a child (for example, an empty block) its child is NULL */
static TreeNode*
get_child(TreeNode* node) {
	switch (node->type) {
		case NODE_IF:
			return node->ifval->child;
		case NODE_FOR:
			return node->forval->child;
		case NODE_WHILE:
			return node->whileval->child;
		case NODE_BLOCK:
			return node->blockval->child;
		case NODE_FUNCTION:
			return node->funcval->child;
		default:
			return 0;
	}
}

/* advances C->at, returns 1 if advanced, 0 if didn't (end of tree) */
static int
advance(CompileState* C) {
	/* jump to the child if it exists */
	TreeNode* child;
	if ((child = get_child(C->at))) {
		C->at = child;
		return 1;
	}
	/* jump to the next node in the block if it exists */
	if (C->at->next) {
		C->at = C->at->next;	
		return 1;
	}
	/* otherwise jump upwards until there is a next node in that block */	
	C->at = C->at->parent;
	while (C->at) {
		if (C->at->next) {
			C->at = C->at->next;
			return 1;
		}
		popb(C);
		C->at = C->at->parent;
	}
	/* didn't advance */
	return 0;
}

static void
generate_function(CompileState* C) {
	TreeFunction* func = C->at->funcval;
	C->return_label = C->label_count++;
	outb(C, FORMAT_FUNCTION_HEAD, func->identifier); /* write function label */
	outb(C, "res %d\n",	func->stack_space); /* reserve bytes for local vars */
	pushb(C, FORMAT_LABEL_HEAD, C->return_label);
	pushb(C, "iret\n"); /* return instruction */
}

static void
generate_if(CompileState* C) {
	generate_expression(C, C->at->ifval->condition);
}

/* passes assembly into the writer function specified by C->write....
 * NOTE: no typechecking needs to be done, that was done by the parser */
static void
generate_expression(CompileState* C, ExpNode* expression) {
	/* just assign ins here so that it doesn't need to be done in
	 * the next set of cases... */
	const VMInstruction* ins = NULL;
	const char* prefix = "";
	switch (expression->type) {
		case EXP_BINOP:
			ins = &arith_instructions[expression->bval->type];
			break;
		case EXP_UNOP:
			ins = &arith_instructions[expression->uval->type];
			break;
	}
	if (ins && ins->has_prefix) {
		/* all types that are not float use the prefix 'i', so just
		 * set it to 'f' if the typename is float */
		printf("%p\n", expression->evaluated_type);
		if (strcmp(expression->evaluated_type->type_name, "float")) {
			prefix = "i";
		} else {
			prefix = "f";
		}
	}
	/* now do the assembly writing */
	switch (expression->type) {
		case EXP_BINOP:
			generate_expression(C, expression->bval->left);
			generate_expression(C, expression->bval->right);
			switch (expression->bval->type) {
				case TOK_PLUS:
					C->write(C, "%sadd\n", prefix);
					break;
				case TOK_HYPHON:
					C->write(C, "%ssub\n", prefix);
					break;
				case TOK_ASTER:
					C->write(C, "%smul\n", prefix);
					break;
				case TOK_FORSLASH:
					C->write(C, "%sdiv\n", prefix);
					break;
			}
			break;
		case EXP_INTEGER:
			C->write(C, "ipush %d\n", expression->ival);
			break;
		case EXP_FLOAT:
			break;
	}
}

void
generate_bytecode(TreeNode* root, const char* outfile) {
	CompileState* C = malloc(sizeof(CompileState));
	C->root_node = root;
	C->at = root;
	C->write = outb;
	C->handle = fopen(outfile, "wb");
	C->label_count = 0;
	C->return_label = 0;
	if (!C->handle) {
		printf("couldn't open file '%s' for writing", outfile);
	}

	do {
		switch (C->at->type) {
			case NODE_IF:
				generate_if(C);
				break;
			case NODE_FUNCTION:
				generate_function(C);
				break;
			case NODE_FOR:
			case NODE_WHILE:
			case NODE_STATEMENT:
			case NODE_BLOCK:
			case NODE_RETURN:
			case NODE_BREAK:
			case NODE_CONTINUE:
				break;
		}
	} while (advance(C));

	fclose(C->handle);
}
