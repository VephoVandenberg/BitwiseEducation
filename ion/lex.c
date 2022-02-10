#include "common.c"

typedef enum tokeKind
{
	TOKEN_EOF = 0,
	// Reserve first 128 values for one-char tokens
	TOKEN_LAST_CHAR = 127,
	TOKEN_INT,
	TOKEN_FLOAT,
	TOKEN_STR,
	TOKEN_NAME,
	TOKEN_LSHIFT,
	TOKEN_RSHIFT,
	TOKEN_EQ,
	TOKEN_NOTEQ,
	TOKEN_LTEQ,
	TOKEN_GTEQ,
	TOKEN_AND,
	TOKEN_OR,
	TOKEN_INC,
	TOKEN_DEC,
	TOKEN_COLON_ASSIGN,
	TOKEN_ADD_ASSIGN,
	TOKEN_SUB_ASSIGN,
	TOKEN_OR_ASSIGN,
	TOKEN_AND_ASSIGN,
	TOKEN_XOR_ASSIGN,
	TOKEN_LSHIFT_ASSIGN,
	TOKEN_RSHIFT_ASSIGN,
	TOKEN_MUL_ASSIGN,
	TOKEN_DIV_ASSIGN,
	TOKEN_MOD_ASSIGN,
} tokenKind;

typedef enum tokenMod
{
	TOKENMOD_NONE,
	TOKENMOD_HEX,
	TOKENMOD_BIN,
	TOKENMOD_OCT,
	TOKENMOD_CHAR,
} tokenMod;

size_t copy_token_kind_str(char* dest, size_t dest_size, tokenKind kind)
{
	size_t n = 0;
	switch (kind)
	{
	case 0:
	{
		n = snprintf(dest, dest_size, "end of file");
	} break;
	case TOKEN_INT:
	{
		n = snprintf(dest, dest_size, "integer");
	} break;
	case TOKEN_NAME:
	{
		n = snprintf(dest, dest_size, "name");
	} break;
	default:
	{
		if (kind < 128 && isprint(kind))
		{
			n = snprintf(dest, dest_size, "%c", kind);
		}
		else
		{
			n = snprintf(dest, dest_size, "<ASCII %d>", kind);
		}
	} break;
	}

	return n;
}

const char* temp_token_kind_str(tokenKind kind)
{
	static char buf[256];
	size_t n = copy_token_kind_str(buf, sizeof(buf), kind);
	assert(n + 1 <= sizeof(buf));
	return buf;
}

typedef struct token
{
	tokenKind kind;
	tokenMod mod;
	const char* start;
	const char* end;
	union
	{
		uint64_t int_val;
		double float_val;
		const char* str_val;
		const char* name;
	};
}token;

token tok;
const char* stream;

uint8_t char_to_digit[256] = {
	['0'] = 0,
	['1'] = 1,
	['2'] = 2,
	['3'] = 3,
	['4'] = 4,
	['5'] = 5,
	['6'] = 6,
	['7'] = 7,
	['8'] = 8,
	['9'] = 9,
	['a'] = 10,['A'] = 10,
	['b'] = 11,['B'] = 11,
	['c'] = 12,['C'] = 12,
	['d'] = 13,['D'] = 13,
	['e'] = 14,['E'] = 14,
	['f'] = 15,['F'] = 15,
};

void scan_int()
{
	uint64_t base = 10;
	if (*stream == '0')
	{
		stream++;
		if (tolower(*stream) == 'x')
		{
			stream++;
			tok.mod = TOKENMOD_HEX;
			base = 16;
		}
		else if (tolower(*stream) == 'b')
		{
			stream++;
			tok.mod = TOKENMOD_HEX;
			base = 2;
		}
		else if (isdigit(*stream))
		{
			tok.mod = TOKENMOD_OCT;
			base = 8;
		}
	}
	uint64_t val = 0;
	for (;;)
	{
		uint64_t digit = char_to_digit[*stream];
		if (digit == 0 && *stream != '0')
		{
			break;
		}
		if (digit >= base)
		{
			syntax_error("Digit '%c' out of range for base %llu", *stream, base);
			digit = 0;
		}
		if (val > (UINT64_MAX - digit) / base)
		{
			syntax_error("Integer overflow");
			while (isdigit(*stream))
			{
				stream++;
			}
			val = 0;
		}
		val = val * base + digit;
		stream++;
	}
	tok.kind = TOKEN_INT;
	tok.int_val = val;
}

void scan_float()
{
	const char* start = stream;
	while (isdigit(*stream))
	{
		stream++;
	}

	if (*stream == '.')
	{
		stream++;
	}

	while (isdigit(*stream))
	{
		stream++;
	}	

	while (tolower(*stream) == 'e')
	{
		stream++;
		if (*stream == '+' || *stream == '-')
		{
			stream++;
		}
		if (!isdigit(*stream))
		{
			syntax_error("Expected digit after float literl exponent, found '%c'.", *stream);
		}
		while (isdigit(*stream))
		{
			stream++;
		}
	}

	const char* end = stream;
	double val = strod(start, NULL);
	if (val == HUGE_VAL || val == -HUGE_VAL)
	{
		syntax_error("float literal overflow");
	}
	tok.kind = TOKEN_FLOAT;
	tok.float_val = val;
}

char escape_to_char[256] = {
	['n'] = '\n',
	['r'] = '\r',
	['t'] = '\t',
	['v'] = '\v',
	['b'] = '\b',
	['a'] = '\a',
	[0] = 0
};

void scan_char()
{
	assert(*stream == '\'');
	stream++;
	char val = 0;
	if (*stream == '\'')
	{
		syntax_error("Char literal cannot be empty");
		stream++;
	}
	else if (*stream == '\n')
	{
		syntax_error("Char literal cannot contain newline");
	}
	else if (*stream == '\\')
	{
		stream++;
		val = escape_to_char[*stream];
		if (val == 0 && *stream != '0')
		{
			syntax_error("Invalid char literal escape '\\%c'", *stream);
		}
		stream++;
	}
	else
	{
		val = *stream;
		stream++;
	}

	if (*stream != '\'')
	{
		syntax_error("Expected closing char quote, got '%c'", *stream);
	}
	else
	{
		stream++;
	}

	tok.kind = TOKEN_INT;
	tok.int_val = val;
	tok.mod = TOKENMOD_CHAR;
}

void scan_str()
{
	assert(*stream == '"');
	stream++;
	char* str = NULL;
	while (*stream && *stream != '"')
	{
		char val = *stream;
		if (val == '\n')
		{
			syntax_error("String literal cannot contain newline");
		}
		else if (val == '\\')
		{
			stream++;
			val = escape_to_char[*stream];
			if (val == 0 && *stream != '0')
			{
				syntax_error("invalid string literal escape '%c'", *stream);
			}
		}
		buf_push(str, val);
		stream++;
	}
	if (*stream)
	{
		assert(*stream == '"');
		stream++;
	}
	else
	{
		syntax_error("Unexpected end of file within string literal");
	}
	buf_push(str, 0);
	tok.kind = TOKEN_STR;
	tok.str_val = str;
}

#define CASE1(c, c1, k1) \
    case c: \
        tok.kind = *stream++; \
        if (*stream == c1) { \
            tok.kind = k1; \
            stream++; \
        } \
        break;

#define CASE2(c, c1, k1, c2, k2) \
    case c: \
        tok.kind = *stream++; \
        if (*stream == c1) { \
            tok.kind = k1; \
            stream++; \
        } else if (*stream == c2) { \
            tok.kind = k2; \
            stream++; \
        } \
        break;


void next_token()
{
top:
	tok.start = stream;
	tok.mod = 0;
	switch (*stream) 
	{
	case ' ': case '\n': case '\r': case '\t': case '\v':
		while (isspace(*stream))
		{
			stream++;
		}
		goto top;
		break;
	case '\'':
		scan_char();
		break;
	case '"':
		scan_str();
		break;
	case '.':
		scan_float();
		break;
	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
		while (isdigit(*stream)) {
			stream++;
		}
		char c = *stream;
		stream = tok.start;
		if (c == '.' || tolower(c) == 'e')
		{
			scan_float();
		}
		else
		{
			scan_int();
		}
		break;
	}
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
	case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
	case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
	case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
	case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_':
		while (isalnum(*stream) || *stream == '_') {
			stream++;
		}
		tok.kind = TOKEN_NAME;
		tok.name = str_intern_range(tok.start, stream);
		break;
	case '<':
		tok.kind = *stream++;
		if (*stream == '<')
		{
			tok.kind = TOKEN_LSHIFT;
			stream++;
			if (*stream == '=')
			{
				tok.kind = TOKEN_LSHIFT_ASSIGN;
				stream++;
			}
		}
		else if (*stream == '=')
		{
			tok.kind = TOKEN_LTEQ;
			stream++;
		}
		break;
	case '>':
		tok.kind = *stream++;
		if (*stream == '>')
		{
			tok.kind = TOKEN_RSHIFT;
			stream++;
			if (*stream == '=')
			{
				tok.kind = TOKEN_RSHIFT_ASSIGN;
				stream++;
			}
		}
		else if (*stream == '=')
		{
			tok.kind = TOKEN_GTEQ;
			stream++;
		}
		break;
	CASE1('^', '=', TOKEN_XOR_ASSIGN)
	CASE1(':', '=', TOKEN_COLON_ASSIGN)
	CASE1('*', '=', TOKEN_MUL_ASSIGN)
	CASE1('/', '=', TOKEN_DIV_ASSIGN)
	CASE1('%', '=', TOKEN_MOD_ASSIGN)
	CASE2('+', '=', TOKEN_ADD_ASSIGN, '+', TOKEN_INC)
	CASE2('-', '=', TOKEN_SUB_ASSIGN, '-', TOKEN_DEC)
	CASE2('&', '=', TOKEN_AND_ASSIGN, '&', TOKEN_AND)
	CASE2('|', '=', TOKEN_OR_ASSIGN, '|', TOKEN_OR)
	default:
		tok.kind = *stream++;
		break;
	}

	tok.end = stream;
}

void init_stream(const char* str)
{
	stream = str;
	next_token();
}

void print_token(token tokObj)
{
	switch (tokObj.kind)
	{
	case TOKEN_INT:
	{
		printf("TOKEN_INT: %d.\n", tokObj.int_val);
	}break;
	case TOKEN_NAME:
	{
		printf("TOKENM_NAME: %.*s.\n", (int)(tokObj.end - tokObj.start), tokObj.start);
	}break;
	default:
	{
		printf("TOKEN '%c.'\n", tokObj.kind);
	}break;
	}
}

static inline bool is_token(tokenKind kind)
{
	return tok.kind == kind;
}

static inline bool is_token_name(const char* name)
{
	return tok.kind == TOKEN_NAME && tok.name == name;
}

static inline bool match_token(tokenKind kind)
{
	if (is_token(kind))
	{
		next_token();
		return true;
	}
	else
	{
		return false;
	}
}

static inline bool expect_token(tokenKind kind)
{
	if (is_token(kind))
	{
		next_token();
		return true;
	}
	else
	{
		char buf[256];
		copy_token_kind_str(buf, sizeof(buf), kind);
		fatal("expected token %s, got %s", buf, temp_token_kind_str(tok.kind));
		return false;
	}
}

#define assert_token(x) assert(match_token(x))
#define assert_token_name(x) assert(tok.name == str_intern(x) && match_token(TOKEN_NAME))
#define assert_token_int(x) assert(tok.int_val == (x) && match_token(TOKEN_INT))
#define assert_token_eof() assert(is_token(0))


void lex_test()
{
	char* source = "XY+(XY)_HELLO1,234+FOO!994";
	stream = source;
	next_token();
	while (tok.kind)
	{
		print_token(tok);
		next_token();
	}
	const char* str = "XY+(XY)_HELLO1,234+994";
	init_stream(str);
	assert_token_name("XY");
	assert_token('+');
	assert_token('(');
	assert_token_name("XY");
	assert_token(')');
	assert_token_name("_HELLO1");
	assert_token(',');
	assert_token_int(234);
	assert_token('+');
	assert_token_int(994);
	assert_token_eof();
}

#undef assert_token_eof
#undef assert_token_int
#undef assert_token_name
#undef assert_token

/*
	Expression grammar:


	expr3 = INT | '(' expr ')'
	expr2 = '-' ? expr2 | expr3
	expr1 = expr2 ([/*] expr2)*
	expr0 = expr1 ([+-] expr1)*
	expr = expr0

*/

int parse_expr();

int parse_expr3()
{
	if (is_token(TOKEN_INT))
	{
		int val = tok.int_val;
		next_token();
		return val;
	}
	else if (match_token('('))
	{
		int val = parse_expr();
		expect_token(')');
		return val;
	}
	else
	{
		fatal("expected integer or (, got %s", temp_token_kind_str(tok.kind));
		return 0;
	}
}

int parse_expr2()
{
	if (match_token('-'))
	{
		return -parse_expr2();
	}
	else if (match_token('+'))
	{
		return parse_expr2();
	}
	else
	{
		return parse_expr3();
	}
}

int parse_expr1()
{
	int val = parse_expr2();
	while (is_token('*') || is_token('/'))
	{
		char op = tok.kind;
		next_token();
		int rval = parse_expr2();
		if (op == '*')
		{
			val *= rval;
		}
		else
		{
			assert(op == '/');
			assert(rval != 0);
			val /= rval;
		}
	}

	return val;
}

int parse_expr0()
{
	int val = parse_expr1();
	while (is_token('+') || is_token('-'))
	{
		char op = tok.kind;
		next_token();
		int rval = parse_expr1();
		if (op == '+')
		{
			val += rval;
		}
		else
		{
			val -= rval;
		}
	}
	return val;
}

int parse_expr()
{
	return parse_expr0();
}

int parse_expr_str(const char* str)
{
	init_stream(str);
	return parse_expr();
}

#define assert_expr(x) assert(parse_expr_str(#x) == (x))

void parse_test()
{
	assert_expr(1);
	assert_expr((1));
	assert_expr(-+1);
	assert_expr(1 - 2 - 3);
	assert_expr(2 * 3 + 4 * 5);
	assert_expr(2 * (3 + 4) * 5);
	assert_expr(2 + -3);
}

#undef assert_expr; 
