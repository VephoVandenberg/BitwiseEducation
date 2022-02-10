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
#include <climits>
#include <math.h>

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
	void* ptr = malloc(num_bytes);
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
#define buf_end(b) ((b) + buf_len(b))
#define buf_push(b, ...) (buf__fit(b, 1), (b)[buf__hdr(b)->len++] = (__VA_ARGS__))
#define buf_free(b) ((b) ? free(buf__hdr(b)) : 0)

void* buf__grow(const void* buf, size_t new_len, size_t elem_size)
{
	assert(buf_cap(buf) <= (SIZE_MAX - 1) / 2);
	size_t new_cap = MAX(1 + 2 * buf_cap(buf), new_len);
	assert(new_len <= new_cap);
	assert(new_cap <= (SIZE_MAX - offsetof(bufHdr, buf)) / elem_size);
	size_t new_size = offsetof(bufHdr, buf) + new_cap * elem_size;
	bufHdr* new_hdr;
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
	int* buf = NULL;
	int n = 1024;
	for (int i = 0; i < n; i++)
	{
		buf_push(buf, i);
	}
	assert(buf_len(buf) == n);

	for (int i = 0; i < buf_len(buf); i++)
	{
		assert(buf[i] == i);
	}
	buf_free(buf);
}

typedef struct intern
{
	size_t len;
	const char* str;
} intern;

static intern* interns;

const char* str_intern_range(const char* start, const char* end)
{
	size_t len = end - start;
	for (intern* it = interns; it != buf_end(interns); it++)
	{
		if (it->len == len && strncmp(it->str, start, len) == 0)
		{
			return it->str;
		}
	}

	char* str = xmalloc(len + 1);
	memcpy(str, start, len);
	str[len] = 0;
	buf_push(interns, ((intern) { len, str }));
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
	assert(strcmp(x, str_intern(x)) == 0);
	assert(str_intern(x) == str_intern(x));
	char y[] = "hello";
	assert(x != y);
	assert(str_intern(x) == str_intern(y));
	char z[] = "hello";
	assert(str_intern(z) == str_intern(x));
	char c[] = "hell";
	char* px = str_intern(x);
	char* pc = str_intern(c);
	assert(str_intern(z) != str_intern(c));
}

void common_test()
{
	buf_test();
	str_intern_test();
}