#ifndef LEX_H
#define LEX_H

typedef struct Lexer Lexer;
typedef struct Token Token;
typedef enum TokenType TokenType;

enum TokenType {
	TOK_IF = 1,
	TOK_ELSE = 2,
	TOK_WHILE = 3,
	TOK_DO = 4,
	TOK_FUNCTION = 5,
	TOK_RETURN = 6,
	TOK_SWITCH = 7,
	TOK_CASE = 8,
	TOK_CONTINUE = 9,
	TOK_BREAK = 10,
	TOK_FOR = 11,
	TOK_IDENTIFIER = 12,
	TOK_INT = 13,
	TOK_STRING = 14,
	TOK_FUNCCALL = 15,
	TOK_STRUCT = 16,
	TOK_FLOAT = 17,
	TOK_CFUNC = 18,
	TOK_CAST = 19,
	TOK_ELIF = 20,
	TOK_SPACE = 32,
	TOK_EXCL = 33,
	TOK_DQUOTE = 34,
	TOK_POUND = 35,
	TOK_DOLLAR = 36,
	TOK_PERCENT = 37,
	TOK_AMPERSAND = 38,
	TOK_QUOTE = 39,
	TOK_OPENPAR = 40,
	TOK_CLOSEPAR = 41,
	TOK_ASTER = 42,
	TOK_PLUS = 43,
	TOK_COMMA = 44,
	TOK_HYPHON = 45,
	TOK_PERIOD = 46,
	TOK_FORSLASH = 47,
	TOK_COLON = 58,
	TOK_SEMICOLON = 59,
	TOK_LT = 60,
	TOK_ASSIGN = 61,
	TOK_GT = 62,
	TOK_QUESTION = 63,
	TOK_AT = 64,
	TOK_OPENSQ = 91,
	TOK_BACKSLASH = 92,
	TOK_CLOSESQ = 93,
	TOK_UPCARROT = 94,
	TOK_UNDERSCORE = 95,
	TOK_IFORGOTLOL = 96,
	TOK_DOTS = 97,
	TOK_OPENCURL = 123,
	TOK_LINE = 124,
	TOK_CLOSECURL = 125,
	TOK_TILDE = 126,
	TOK_LOGAND = 128,
	TOK_LOGOR = 129,
	TOK_SHR = 130,
	TOK_SHL = 131,
	TOK_INC = 132,
	TOK_INCBY = 133,
	TOK_DEC = 134,
	TOK_DECBY = 135,
	TOK_MULBY = 136,
	TOK_DIVBY = 137,
	TOK_MODBY = 138,
	TOK_ANDBY = 139,
	TOK_ORBY = 140,
	TOK_XORBY = 141,
	TOK_SHRBY = 142,
	TOK_SHLBY = 143,
	TOK_ARROWBY = 144,
	TOK_EQ = 145,
	TOK_NOTEQ = 146,
	TOK_GE = 147,
	TOK_LE = 148,
	TOK_ARROW = 149,
	TOK_IGNORE = 200
};

struct Token {
	char*			word;
	unsigned int	line;	
	int	type;

	Token*			next;
	Token*			prev;
};

Token* generate_tokens(const char*);
void append_token(Token*, char*, unsigned int, unsigned int);
void print_tokens(Token*);
char tt_to_word(TokenType);
Token* blank_token();

#endif
