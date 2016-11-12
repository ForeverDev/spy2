#ifndef GENERATE_H
#define GENERATE_H

#include "parse.h"

typedef struct CompileState CompileState;
typedef void (*writer)(CompileState*, const char*, ...);

struct CompileState {
	TreeNode* root_node;
	writer write;
	FILE* handle;

};

void generate_bytecode(TreeNode*, const char*);

#endif
