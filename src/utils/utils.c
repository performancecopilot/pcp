#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void die (int line_number, const char* format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    fprintf(stderr, "%d: ", line_number);
    vfprintf(stderr, format, vargs);
    printf("\n");
    fprintf(stderr, ".\n");
    va_end(vargs);
    exit(1);
}