#include <string.h>
#include <stdlib.h>
#include "spyre.h"
#include "assembler.h"
#include "lex.h"
#include "parse.h"
#include "generate.h"

int correct_suffix(const char* str) {
	size_t len = strlen(str);
	if (len <= 4) {
		return 0;
	}
	return !strncmp(&str[len - 4], ".spy", 4);
}

int main(int argc, char** argv) {

	if (argc <= 1) return printf("expected file name\n");

	char* args[] = {argv[1]};

	const unsigned int flags = SPY_NOFLAG;

	ParseOptions options;
	options.opt_level = OPT_THREE;
	//options.opt_level = OPT_ZERO;
	
	if (strlen(argv[1]) == 1) {
	
		if (argc < 3) {
			printf("expected file name\n");
			exit(1);
		}

		size_t flen = strlen(argv[2]);
		char* outfile = malloc(flen + 2);
		strcpy(outfile, argv[2]);
		outfile[flen] = 's'; /* convert the output name to *.spys form */
		outfile[flen + 1] = 0;

		if (!strncmp(argv[1], "a", 1)) {
			Assembler_generateBytecodeFile(argv[2]);
		} else if (!strncmp(argv[1], "r", 1)) {
			Spy_execute(argv[2], flags, 1, args);
		} else if (!strncmp(argv[1], "c", 1)) {
			if (!correct_suffix(argv[2])) {
				printf("expected Spyre source file\n");
				exit(1);
			}	
			LexState* tokens = generate_tokens(argv[2]);	
			TreeNode* tree = generate_tree(tokens, &options);
			generate_bytecode(tree, outfile);
		}
	} else {
		if (!correct_suffix(argv[1])) {
			printf("expected Spyre source file\n");
			exit(1);
		}	
	}

	return 0;

}
