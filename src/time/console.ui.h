/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include <stdarg.h>

void Console::init(int level)
{
    _level = level;
}

void Console::post(int level, char *fmt, ...)
{
    static char buffer[4096];
    va_list ap;

    if (!(_level & level))
	return;

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    fputs(buffer, stderr);
    fputc('\n', stderr);
    text->append(QString(buffer));
}
