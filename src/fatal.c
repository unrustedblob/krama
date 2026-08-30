// Implementation of the fatal exit paths

#include "fatal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

[[noreturn]] void fatal(enum FatalPath action, const char *msg, ...)
{
        va_list args;
        va_start(args, msg);
        vfprintf(stderr, msg, args);
        va_end(args);
        fprintf(stderr, "\n");

        if (action == ABORT) {
                abort();
        }
        exit(1);
}
