#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>


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
	char buf[0];
} bufHdr;

#define buf__hdr(b) ((bufHdr *)((char *)b - offsetof(bufHdr, buf)))
#define buf__fits(b, n) (buf_len(b) + (n) <= buf_cap(b))
#define buf__fit(b, n) (buf__fits(b, n) ? 0 : ((b) = buf__grow((b), buf_len(b) + (n), sizeof(*(b)))))

#define buf_len(b) ((b) ? buf__hdr(b)->len : 0)
#define buf_cap(b) ((b) ? buf__hdr(b)->cap : 0)
#define buf_push(b, x) (buf__fit(b, 1), (b)[buf__hdr(b)->len++] = (x))
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
	int *asdf = NULL;
	enum {N = 1024};
	for (int i = 0; i < N; i++)
	{
		buf_push(asdf, i);
	}
	assert(buf_len(asdf) == N);

	for (int i = 0; i < buf_len(asdf); i++)
	{
		assert(asdf[i] == i);
	}
	buf_free(asdf);
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
	TOKEN_INT = 128,
	TOKEN_NAME, 
	// ...
} tokenKind;

typedef struct token
{
	tokenKind kind;
	union
	{
		uint64_t val;
		struct
		{
			const char* start;
			const char* end;
		};
	};
}token;

token tok;
const char* stream;

void next_token()
{
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
		tok.start = start;
		tok.end = stream;
	}
	else
	{
		tok.kind = *stream++;
	}
}

void print_token(token tokObj)
{
	switch (tokObj.kind)
	{
	case TOKEN_INT:
	{
		printf("TOKEN_INT: %llu.\n", tokObj.val);
	}break;
	case TOKEN_NAME:
	{
		printf("TOKENM_NAME: %.*s.\n", (int)(tokObj.end - tokObj.start), tokObj.start);
	}break;
	default:
	{
		printf("TOKEN '%c'\n", tokObj.kind);
	}break;
	}
}

void lex_test()
{
	char *source = "+()_HELLO1,234+FOO!994";
	stream = source;
	next_token();
	while (tok.kind)
	{
		print_token(tok);
		next_token();
	}

}

int main(int argc, char **argv)
{
	buf_test();
	lex_test();
	str_intern_test();
	return 0;
}
