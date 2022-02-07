#define _CRT_SECURE_NO_WARNINGS
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#define MAX(x, y) ((x) >= (y) ? (x) : (y)) 

void* xrealloc(void* ptr, size_t num_bytes)
{
	ptr = realloc(ptr, num_bytes);
	if (!ptr)
	{
		perror("Realloc failed");
		exit(1);
	}
		
	return ptr;
}

void* xmalloc(size_t num_bytes)
{
	void *ptr = malloc(num_bytes);
	if (!ptr)
	{
		perror("Malloc failed");
		exit(1);
	}

	return ptr;
}

void fatal(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("FATAL: ");
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	exit(1);
}

// Stretchy buffers by Sean Barret

typedef struct bufHdr
{
	size_t len;
	size_t cap;
	char buf[];
} bufHdr;

#define buf__hdr(b) ((bufHdr *)((char *)b - offsetof(bufHdr, buf)))
#define buf__fits(b, n) (buf_len(b) + (n) <= buf_cap(b))
#define buf__fit(b, n) (buf__fits(b, n) ? 0 : ((b) = buf__grow((b), buf_len(b) + (n), sizeof(*(b)))))

#define buf_len(b) ((b) ? buf__hdr(b)->len : 0)
#define buf_cap(b) ((b) ? buf__hdr(b)->cap : 0)
#define buf_push(b, ...) (buf__fit(b, 1), (b)[buf__hdr(b)->len++] = (__VA_ARGS__))
#define buf_free(b) ((b) ? free(buf__hdr(b)) : 0)

void *buf__grow(const void* buf, size_t new_len, size_t elem_size)
{
	size_t new_cap = MAX(1 + 2*buf_cap(buf), new_len);
	assert(new_len <= new_cap);
	size_t new_size = offsetof(bufHdr, buf) + new_cap*elem_size;
	bufHdr *new_hdr;
	if (buf)
	{
		new_hdr = xrealloc(buf__hdr(buf), new_size);
	}
	else
	{
		new_hdr = xmalloc(new_size);
		new_hdr->len = 0;
	}
	new_hdr->cap = new_cap;
	return new_hdr->buf;
}

void buf_test()
{
	int *buf = NULL;
	enum {N = 1024};
	for (int i = 0; i < N; i++)
	{
		buf_push(buf, i);
	}
	assert(buf_len(buf) == N);

	for (int i = 0; i < buf_len(buf); i++)
	{
		assert(buf[i] == i);
	}
	buf_free(buf);
}

typedef struct internStr
{
	size_t len;
	const char* str;
} internStr;

static internStr* interns;

const char* str_intern_range(const char *start, const char *end)
{
	size_t len = end - start;
	for (size_t i = 0; i < buf_len(interns); i++)
	{
		if (interns[i].len = len && strncmp(interns[i].str, start, len) == 0)
		{
			return interns[i].str;
		}
	}

	char* str = xmalloc(len + 1);
	memcpy(str, start, len);
	str[len] = 0;
	buf_push(interns, ((internStr) { len, str }));
	return str;
}

// Lexing: translating char stream to token stream
const char* str_intern(const char* str)
{
	return str_intern_range(str, str + strlen(str));
}

void str_intern_test()
{
	char x[] = "hello";
	char y[] = "hello";
	assert(x != y);
	const char* px = str_intern(x);
	const char* py = str_intern(y);
	assert(px == py);
	char z[] = "hello";
	const char* pz = str_intern(z);
	assert(pz == px);
}


typedef enum tokenKind
{
	TOKEN_LAST_CHAR = 127,
	TOKEN_INT,
	TOKEN_NAME
	// ...
} tokenKind;

typedef enum nameKind
{
	/*
	'+'|'-'|'^'|'|'
	*/
	ADD,
	SUB,
	XOR,
	OR,
	
	/*
	'*'|'/'|'>>'|'<<'|'%'|'&'
	*/
	MUL,
	DIV,
	RSH,
	LSH,
	MOD,
	AND,

	/*
	'-'|'~'
	*/
	NEG,
	NOT
} nameKind;

const char* token_kind_name(tokenKind kind)
{
	static char buf[256];
	switch (kind)
	{
	case TOKEN_INT:
	{
		sprintf(buf, "integer");
	} break;
	case TOKEN_NAME:
	{
		sprintf(buf, "name");
	} break;
	default:
	{
		if (kind < 128 && isprint(kind))
		{
			sprintf(buf, "%c", kind);
		}
		else
		{
			sprintf(buf, "<ASCII %d>", kind);
		}
	}break;
	}

	return buf;
}

typedef struct token
{
	tokenKind kind;
	const char* start;
	const char* end;
	union
	{
		int val;
		const char* name;
	};
}token;

token tok;
const char* stream;

const char* keyword_if;
const char* keyword_for;
const char* keyword_while;

void init_keywords()
{
	keyword_if = str_intern("if");
	keyword_for = str_intern("for");
	keyword_while = str_intern("while");
}

void next_token()
{	
	tok.start = stream;
	if (*stream >= '0' && *stream <= '9')
	{
		uint64_t val = 0;
		while (isdigit(*stream))
		{
			val *= 10;
			val += *stream++ - '0';
		}
		tok.kind = TOKEN_INT;
		tok.val = val;
	}
	else if (*stream >= 'a' && *stream <= 'z' ||
		*stream >= 'A' && *stream <= 'Z' || 
		*stream == '_')
	{
		const char* start = stream++;
		while (isalnum(*stream) || *stream == '_')
		{
			stream++;
		}
		tok.kind = TOKEN_NAME;
		tok.name = str_intern_range(tok.start, stream);
	}
	else
	{
		tok.kind = *stream++;
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
		printf("TOKEN_INT: %d.\n", tokObj.val);
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
		fatal("expected token %s, got %s", token_kind_name(kind), token_kind_name(tok.kind));
		return false;
	}
}


void lex_test()
{
	char *source = "XY+(XY)_HELLO1,234+FOO!994";
	stream = source;
	next_token();
	while (tok.kind)
	{
		print_token(tok);
		next_token();
	}
}

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
		int val = tok.val;
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
		fatal("expected integer or (, got %s", token_kind_name(tok.kind));
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

#define TEST_EXPR(x) assert(parse_expr_str(#x) == (x))

void parse_test()
{
	TEST_EXPR(1);
	TEST_EXPR(1+2);
}

#undef TEST_EXPR; 

// Virtual machine

#define POP() *(--top)
#define PUSH(x) (*top++ = (x))
#define POPS(n) assert(top - stack >= (n))
#define PUSHES(n) assert(top + (n) <= stack + MAX_STACK)

int32_t vm_exec(const uint8_t *code)
{
	enum {MAX_STACK = 1024};
	int32_t stack[MAX_STACK];
	int32_t* top = stack;
	for (;;)
	{
		uint8_t op = *code++;

		switch (op)
		{
		case SUB:
		{

		} break;

		case ADD:
		{
			break;
		}
		}
	}
}

void vm_test()
{

}

int main(int argc, char **argv)
{
	buf_test();
	lex_test();
	str_intern_test();
	parse_test();
	vm_test();
	return 0;
}
