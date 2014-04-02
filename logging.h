#ifndef LOGGING_H
#define LOGGING_H

#include <stdlib.h>
#include <stdarg.h>

extern int loglevel;

void debug(int , const char *, ...)
	__attribute__((format(printf, 2, 3)));

void hexdump(int , const unsigned char *, size_t );
#endif /* LOGGING_H */
