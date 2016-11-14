#ifndef GENERATE_H
#define GENERATE_H

#include "parse.h"

typedef struct CompileState CompileState;
typedef struct InstructionStack InstructionStack;
typedef void (*writer)(CompileState*, const char*, ...);

struct InstructionStack {
	TreeNode* correspond; /* the corresponding code node */	
	LiteralList* instructions; /* list of instructions to append */
	InstructionStack* next;
	InstructionStack* prev;
};

struct CompileState {
	TreeNode* root_node;			/* top of the tree */
	TreeNode* at;					/* node currently being generated */
	writer write;					/* function used to write bytes (outb or pushb) */
	FILE* handle;					/* outfile handle */
	unsigned int label_count;		/* current label number */
	unsigned int return_label;		/* current return label number */
	InstructionStack* ins_stack;
	
};

void generate_bytecode(TreeNode*, const char*);

#endif
