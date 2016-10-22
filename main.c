#include <string.h>
#include <stdlib.h>
#include "spyre.h"
#include "assembler.h"
#include "lex.h"
#include "parse.h"

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
		if (!strncmp(argv[1], "a", 1)) {
			Assembler_generateBytecodeFile(argv[2]);
		} else if (!strncmp(argv[1], "r", 1)) {
			Spy_execute(argv[2], flags, 1, args);
		} else if (!strncmp(argv[1], "c", 1)) {
			LexState* tokens = generate_tokens(argv[2]);	
			ParseState* tree = generate_tree(tokens, &options);
		}
	} else {
		if (!correct_suffix(argv[1])) {
			printf("expected Spyre source file\n");
			exit(1);
		}	
	}

	return 0;

}
