#ifndef LOGGING_H
#define LOGGING_H

#include <stdlib.h>
#include <stdarg.h>

extern int loglevel;

void debug(int , const char *, ...)
	__attribute__((format(printf, 2, 3)));

#endif /* LOGGING_H */
