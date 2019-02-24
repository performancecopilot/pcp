#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

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

void sanitize_string(char *src) {
    int segment_length = strlen(src);
    int i;
    for (i = 0; i < segment_length; i++) {
        char current_char = src[i];
        if (((int) current_char >= (int) 'a' && (int) current_char <= (int) 'z') ||
            ((int) current_char >= (int) 'A' && (int) current_char <= (int) 'A') ||
            ((int) current_char >= (int) '0' && (int) current_char <= (int) '9') ||
            (int) current_char == (int) '.' ||
            (int) current_char == (int) '_') {
            continue;
        }
        else if ((int) current_char == (int) '/' || 
                 (int) current_char == (int) '-' ||
                 (int) current_char == (int) ' ') {
            src[i] = '_';
        } else {
            die(__LINE__, "Unable to sanitize string");
        }
    }
}