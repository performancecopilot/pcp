#ifndef UTILS_
#define UTILS_

void die(int line_number, const char* format, ...);

void warn(int line_number, const char* format, ...);

void sanitize_string(char *src);

#endif