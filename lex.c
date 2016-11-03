#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "lex.h"

static char* token_map[] = {
	[TOK_NULL] = "null",
	[TOK_IF] = "if",
	[TOK_ELSE] = "else",
	[TOK_WHILE] = "while",
	[TOK_DO] = "do",
	[TOK_FUNCTION] = "func",
	[TOK_RETURN] = "return",
	[TOK_SWITCH] = "switch",
	[TOK_CASE] = "case",
	[TOK_CONTINUE] = "continue",
	[TOK_BREAK] = "break",
	[TOK_FOR] = "for",
	[TOK_IDENTIFIER] = "id",
	[TOK_INT] = "int",
	[TOK_STRING] = "string",
	[TOK_FUNCCALL] = "()",
	[TOK_STRUCT] = "struct",
	[TOK_FLOAT] = "float",
	[TOK_CFUNC] = "cfunc",
	[TOK_CAST] = "cast",
	[TOK_ELIF] = "elif",
	[TOK_SPACE] = "<space>",
	[TOK_EXCL] = "!",
	[TOK_DQUOTE] = "\"",
	[TOK_POUND] = "#",
	[TOK_DOLLAR] = "$",
	[TOK_PERCENT] = "%",
	[TOK_AMPERSAND] = "&",
	[TOK_QUOTE] = "'",
	[TOK_OPENPAR] = "(",
	[TOK_CLOSEPAR] = ")",
	[TOK_ASTER] = "*",
	[TOK_PLUS] = "+",
	[TOK_COMMA] = ",",
	[TOK_HYPHON] = "-",
	[TOK_PERIOD] = ".",
	[TOK_FORSLASH] = "/",
	[TOK_COLON] = ":",
	[TOK_SEMICOLON] = ";",
	[TOK_LT] = "<",
	[TOK_ASSIGN] = "=",
	[TOK_GT] = ">",
	[TOK_QUESTION] = "?",
	[TOK_AT] = "@",
	[TOK_OPENSQ] = "[",
	[TOK_BACKSLASH] = "\\",
	[TOK_CLOSESQ] = "]",
	[TOK_UPCARROT] = "^",
	[TOK_UNDERSCORE] = "_",
	[TOK_DOTS] = "...",
	[TOK_OPENCURL] = "{",
	[TOK_LINE] = "|",
	[TOK_CLOSECURL] = "}",
	[TOK_TILDE] = "~",
	[TOK_LOGAND] = "&&",
	[TOK_LOGOR] = "||",
	[TOK_SHR] = ">>",
	[TOK_SHL] = "<<",
	[TOK_INC] = "++",
	[TOK_INCBY] = "+=",
	[TOK_DEC] = "--",
	[TOK_DECBY] = "-=",
	[TOK_MULBY] = "*=",
	[TOK_DIVBY] = "/=",
	[TOK_MODBY] = "%=",
	[TOK_ANDBY] = "&=",
	[TOK_ORBY] = "|=",
	[TOK_XORBY] = "^=",
	[TOK_SHRBY] = ">>=",
	[TOK_SHLBY] = "<<=",
	[TOK_ARROWBY] = "->=",
	[TOK_EQ] = "==",
	[TOK_NOTEQ] = "!=",
	[TOK_GE] = ">=",
	[TOK_LE] = "<=",
	[TOK_ARROW] = "->",
	[TOK_INFERASSIGN] = ":=",
	[TOK_IGNORE] = "?"
};

void
print_tokens(Token* head) {
	while (head) {
		printf("(%d : %s)\n", head->type, head->word);
		head = head->next;
	}
}

Token*
blank_token() {
	return (Token *)calloc(1, sizeof(Token));
}

char*
tt_to_word(TokenType type) {
	return token_map[type];
}

void
append_token(Token* head, char* word, unsigned int line, TokenType type) {
	if (head->type == 0) {
		head->word = word;
		head->line = line;
		head->type = type;
		head->next = NULL;
		head->prev = NULL;
	} else {
		Token* new = malloc(sizeof(Token));
		while (head->next) {
			head = head->next;
		}
		new->word = word;
		new->line = line;
		new->type = type;
		new->next = NULL;
		new->prev = head;
		head->next = new;
	}
}

LexState*
generate_tokens(const char* filename) {
	
	LexState* lexer = malloc(sizeof(LexState));
	lexer->filename = filename;
	lexer->total_lines = 0;
	lexer->tokens = malloc(sizeof(Token));
	lexer->tokens->next = NULL;
	lexer->tokens->type = 0; /* empty */
	lexer->tokens->line = 0;
	lexer->tokens->word = NULL;

	FILE* handle;
	char* contents;
	unsigned long long flen;
	handle = fopen(filename, "rb");
	fseek(handle, 0, SEEK_END);
	flen = ftell(handle);
	fseek(handle, 0, SEEK_SET);
	contents = malloc(flen + 1);
	fread(contents, 1, flen, handle);
	contents[flen] = 0;

	unsigned int line = 1;
	
	/* general purpose vars */
	char* buf;
	char* start;
	unsigned int len = 0;

	while (*contents) {
		len = 0;
		if (*contents == '\n') {
			contents++;
			line++;
			continue;
		} else if (*contents && *(contents + 1) && *contents == '/' && *(contents + 1) == '*') {
			contents += 2;
			while (*contents && *(contents + 1)) {
				if (*contents == '*' && *(contents + 1) == '/') {
					contents += 2;
					break;
				} else if (*contents == '\n') {
					line++;
				}
				contents++;
			}
		} else if (*contents == '\t' || *contents == 32 || *contents == 13) {
			contents++;
			continue;
		} else if (*contents && *(contents + 1) && *(contents + 2) && !strncmp(contents, "...", 3)) {
			contents += 3;
			buf = calloc(1, 4);
			strcpy(buf, "...");
			buf[3] = 0;
			append_token(lexer->tokens, buf, line, TOK_DOTS);
		} else if (isalpha(*contents) || *contents == '_' || *contents == '"') {
			int is_string = 0;
			if (*contents == '"') {
				is_string = 1;
				contents++;
			}
			start = contents;
			if (is_string) {
				while (*contents != '"') {
					contents++;
					len++;
				}
				contents++;
			} else {
				while (*contents && (isalnum(*contents) || *contents == '_') && *contents != ' ') {
					contents++;
					len++;
				}
			}
			buf = calloc(1, len + 1);
			for (unsigned i = 0; i < contents - start; i++) {
				buf[i] = start[i];
			}
			buf[len] = 0;
			append_token(lexer->tokens, buf, line, (
				is_string ? TOK_STRING : 
				!strcmp(buf, "if") ? TOK_IF : 
				!strcmp(buf, "else") ? TOK_ELSE : 
				!strcmp(buf, "elif") ? TOK_ELIF :
				!strcmp(buf, "while") ? TOK_WHILE :
				!strcmp(buf, "do") ? TOK_DO : 
				!strcmp(buf, "func") ? TOK_FUNCTION :
				!strcmp(buf, "return") ? TOK_RETURN : 
				!strcmp(buf, "switch") ? TOK_SWITCH : 
				!strcmp(buf, "case") ? TOK_CASE : 
				!strcmp(buf, "continue") ? TOK_CONTINUE : 
				!strcmp(buf, "break") ? TOK_BREAK : 
				!strcmp(buf, "for") ? TOK_FOR : 
				!strcmp(buf, "struct") ? TOK_STRUCT : 
				!strcmp(buf, "cfunc") ? TOK_CFUNC : TOK_IDENTIFIER
			));
		} else if (isdigit(*contents)) {
			start = contents;
			/* TODO register all number formats and convert to base 10 */
			int is_float = 0;
			while (isdigit(*contents) || *contents == '.') {
				if (*contents == '.') {
					is_float = 1;
				}
				contents++;
				len++;
			}	
			buf = calloc(1, len + 1);
			strncpy(buf, start, len);
			buf[len] = 0;
			append_token(lexer->tokens, buf, line, is_float ? TOK_FLOAT : TOK_INT);
		} else if (ispunct(*contents)) {
			/* replace with strcmp? */
			#define CHECK2(str) (*contents == str[0] && contents[1] == str[1])
			#define CHECK3(str) (*contents == str[0] && contents[1] == str[1] && contents[2] == str[2])

			unsigned int type;

			start = contents;
			type = (
				CHECK3(">>=") ? 142 :
				CHECK3("<<=") ? 143 :
				CHECK3("->=") ? 144 :
				CHECK2("&&") ? 128 : 
				CHECK2("||") ? 129 :
				CHECK2(">>") ? 130 : 
				CHECK2("<<") ? 131 :
				CHECK2("++") ? 132 :
				CHECK2("+=") ? 133 :
				CHECK2("--") ? 134 :
				CHECK2("-=") ? 135 :
				CHECK2("*=") ? 136 :
				CHECK2("/=") ? 137 :
				CHECK2("%=") ? 138 :
				CHECK2("&=") ? 139 :
				CHECK2("|=") ? 140 :
				CHECK2("^=") ? 141 :
				CHECK2("==") ? 145 :
				CHECK2("!=") ? 146 : 
				CHECK2(">=") ? 147 :
				CHECK2("<=") ? 148 : 
				CHECK2("->") ? 149 : 
				CHECK2(":=") ? 150 : (unsigned int)*contents
			);

			if (type == (unsigned int)*contents) {
				contents++;
			} else if (type >= 142 && type <= 144) {
				contents += 3;
			} else {
				contents += 2;
			}
			
			len = (unsigned int)(contents - start);
			buf = malloc(len + 1);
			strncpy(buf, start, len);
			buf[len] = 0;
			append_token(lexer->tokens, buf, line, type);
		}
	}

	lexer->total_lines = line - 1;

	return lexer;

}
