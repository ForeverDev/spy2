#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "generate.h"

static void 
outb(CompileState* C, const char* format, ...) {
	va_list list;
	char* autobuf;
	va_start(list, format);
	vasprintf(&autobuf, format, list);
	fputs(autobuf, C->handle);
	free(autobuf);
	va_end(list);
}

static void
pushb(CompileState* C, const char* format, ...) {

}

void
generate_bytecode(TreeNode* root, const char* outfile) {
	CompileState* C = malloc(sizeof(CompileState));
	C->root_node = root;
	C->write = outb;
	C->handle = fopen(outfile, "wb");
	if (!C->handle) {
		printf("couldn't open file '%s' for writing", outfile);
	}

	fclose(C->handle);
}
