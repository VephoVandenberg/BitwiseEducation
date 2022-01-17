
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>


#define MAX(x, y) ((x) >= (y) ? (x) : (y)) 
// Stretch buffers by Sean Barret

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
#define buf_push(b, x) (buf__fit(b, 1), b[buf_len(b)] = (x), buf__hdr(b)->len++)
#define buf_free(b) ((b) ? free(buf__hdr(b)) : 0)

void *buf__grow(const void* buf, size_t new_len, size_t elem_size)
{
	size_t new_cap = MAX(1 + 2*buf_cap(buf), new_len);
	assert(new_len <= new_cap);
	size_t new_size = offsetof(bufHdr, buf) + new_cap*elem_size;
	bufHdr *new_hdr;
	if (buf)
	{
		new_hdr = realloc(buf__hdr(buf), new_size);
	}
	else
	{
		new_hdr = malloc(new_size);
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

// Lexing: translating char stream to token stream

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
	printf("%d\n", sizeof(bufHdr));
	buf_test();
	lex_test();
	return 0;
}
