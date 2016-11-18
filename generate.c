#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "generate.h"

#define FORMAT_FUNCTION "__FUNC__%s"
#define FORMAT_LABEL "__LABEL__%04d"
#define FORMAT_JMP "jmp " FORMAT_LABEL "\n"
#define FORMAT_JZ "jz " FORMAT_LABEL "\n"
#define FORMAT_JNZ "jnz " FORMAT_LABEL "\n"
#define FORMAT_COMMENT_NUM ";  %d\n"

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
static void generate_while(CompileState*);
static void generate_for(CompileState*);

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
	unsigned int false_label = C->label_count++;
	generate_expression(C, C->at->ifval->condition);
	outb(C, FORMAT_JZ, false_label);
	pushb(C, FORMAT_LABEL_HEAD, false_label);
}

static void
generate_while(CompileState* C) {
	unsigned int cond_label = C->label_count++;
	unsigned int finish_label = C->label_count++;
	outb(C, FORMAT_LABEL_HEAD, cond_label);
	generate_expression(C, C->at->whileval->condition);
	outb(C, FORMAT_JZ, finish_label);
	pushb(C, FORMAT_JMP, cond_label);
	pushb(C, FORMAT_LABEL_HEAD, finish_label);
}

static void
generate_for(CompileState* C){ 
	unsigned int cond_label = C->label_count++;
	unsigned int finish_label = C->label_count++;
	generate_expression(C, C->at->forval->initializer);
	outb(C, FORMAT_LABEL_HEAD, cond_label);
	generate_expression(C, C->at->forval->condition);
	outb(C, FORMAT_JZ, finish_label);
	C->write = pushb; /* statement should be pushed */ 
	generate_expression(C, C->at->forval->statement);
	C->write = outb;
	pushb(C, FORMAT_JMP, cond_label);
	pushb(C, FORMAT_LABEL_HEAD, finish_label);
}

/* passes assembly into the writer function specified by C->write....
 * NOTE: no typechecking needs to be done, that was done by the parser */
static int is_lhs = 0;

static void
generate_expression(CompileState* C, ExpNode* expression) {
	/* just assign ins here so that it doesn't need to be done in
	 * the next set of cases... */
	const VMInstruction* ins = NULL;
	const char* prefix = "";

	/* if it's an assignent, is_lhs will be true for the call to
	 * the left side.  We only want to halt dereferencing for the
	 * upmost operator.  for example:
	 *
	 * ^(^p + 13) = 16;
	 *
	 * the first '^' should not dereference, we want to write to
	 * that location, however the '^' applied to 'p' SHOULD dereference
	 *
	 */
	int dont_der = is_lhs;
	is_lhs = 0;

	switch (expression->type) {
		case EXP_BINOP:
			ins = &arith_instructions[expression->bval->type];
			break;
		case EXP_UNOP:
			ins = &arith_instructions[expression->uval->type];
			break;
		case EXP_IDENTIFIER:
			if (expression->evaluated_type->parent_var) {
				prefix = !strcmp(expression->evaluated_type->type_name, "float") ? "f" : "i";
			}
			break;
	}
	if (ins && ins->has_prefix) {
		/* all types that are not float use the prefix 'i', so just
		 * set it to 'f' if the typename is float */
		if (strcmp(expression->evaluated_type->type_name, "float")) {
			prefix = "i";
		} else {
			prefix = "f";
		}
	}
	/* now do the assembly writing */
	switch (expression->type) {
		case EXP_IDENTIFIER: 
			if (expression->evaluated_type->parent_var) {
				/* if it has a corresponding parent var it is a local...
				 * therefore it also has an offset */
				TreeVariable* parent_var = expression->evaluated_type->parent_var;
				
				/* only load value if it's not lhs... */
				if (dont_der) {
					C->write(C, "lea %d\n", parent_var->offset/8);
				} else {
					C->write(C, "%slload %d\n", prefix, parent_var->offset/8);
				}
			}
			break;	
		case EXP_BINOP: {
			ExpNode* lhs = expression->bval->left;
			ExpNode* rhs = expression->bval->right;
			switch (expression->bval->type) {
				case TOK_ASSIGN:
					/* get the memory address of the left hand side... */
					is_lhs = 1; /* DONT DEREFERENCE IF THERE IS AN OPERATOR IMPLYING THAT */
					generate_expression(C, lhs);
					
					is_lhs = 0; /* already 0, but might as well be explicit */
					/* generate code for the right hand side... */
					generate_expression(C, rhs);
					
					/* we can safely assume that the lhs of '=' is a memory address..
					 * the parser made sure of this... */
					C->write(C, "%ssave\n", prefix);	
					break;
				default:
					generate_expression(C, lhs);
					generate_expression(C, rhs);
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
						case TOK_GT:
							C->write(C, "%sgt\n", prefix);
							break;
						case TOK_GE:
							C->write(C, "%sge\n", prefix);
							break;
						case TOK_LT:
							C->write(C, "%slt\n", prefix);
							break;
						case TOK_LE:
							C->write(C, "%sle\n", prefix);
							break;
						case TOK_EQ:
							C->write(C, "%scmp\n", prefix);
							break;
					}
					break;
			}
			break;
		}
		case EXP_UNOP:
			generate_expression(C, expression->uval->operand);
			switch (expression->uval->type) {
				case TOK_UPCARROT:
					/* don't dereference if dont_der is set */
					if (dont_der) {
						break;
					}
					C->write(C, "ider\n");
					break;
			}
			break;
		case EXP_CAST: {
			TreeType* target = expression->cval->datatype;
			TreeType* operand = expression->cval->operand->evaluated_type;
			generate_expression(C, expression->cval->operand);
			if (!strcmp(target->type_name, "float") && !strcmp(operand->type_name, "int")) {
				/* convert int to float */	
				C->write(C, "itof 0\n");
			} else if (!strcmp(target->type_name, "int") && !strcmp(operand->type_name, "float")) {
				/* convert float to int */
				C->write(C, "ftoi 0\n");
			}
			break;
		}
		case EXP_INTEGER:
			C->write(C, "ipush %d\n", expression->ival);
			break;
		case EXP_FLOAT:
			C->write(C, "fpush %f\n", expression->fval);
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
	C->ins_stack = NULL;
	if (!C->handle) {
		printf("couldn't open file '%s' for writing", outfile);
	}

	outb(C, "jmp __LABEL__ENTRY\n");

	do {
		outb(C, FORMAT_COMMENT_NUM, C->at->line);
		switch (C->at->type) {
			case NODE_IF:
				generate_if(C);
				break;
			case NODE_FUNCTION:
				generate_function(C);
				break;
			case NODE_STATEMENT:
				generate_expression(C, C->at->stateval);
				break;
			case NODE_WHILE:
				generate_while(C);
				break;
			case NODE_FOR:
				generate_for(C);
				break;
			case NODE_BLOCK:
			case NODE_RETURN:
			case NODE_BREAK:
			case NODE_CONTINUE:
				break;
		}
	} while (advance(C));

	outb(C, "__LABEL__ENTRY:\ncall __FUNC__main\n");

	fclose(C->handle);
}
