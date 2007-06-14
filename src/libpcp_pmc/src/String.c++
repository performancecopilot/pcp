/*
 * Copyright (c) 1997,2005 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: String.c++,v 1.2 2005/05/10 00:46:37 kenmcd Exp $"

#include <stdio.h>
#include "String.h"
#include "Bool.h"

PMC_String::PMC_String(char const* str)
: _len((str == NULL ? 0 : strlen(str))), _str(_len + 1)
{
    if (_len > 0)
	strcpy(_str.ptr(), str);
    else
        _str.ptr()[0] = '\0';
}

PMC_String const&
PMC_String::operator=(char const* ptr)
{
    if (ptr == NULL) {
	_len = 0;
	_str[0] = '\0';
    }
    else if (strcmp(_str.ptr(), ptr) != 0) {
	uint_t len = strlen(ptr);
	
	if (len + 1 > size()) {
	    char *oldPtr = _str.resizeNoCopy(len + 1);
	    delete [] oldPtr;
	}
	
	strcpy(_str.ptr(), ptr);
	_len = len;
    }

    return *this;
}

PMC_String const& 
PMC_String::operator=(PMC_String const& rhs)
{
    if (this != &rhs && strcmp(_str.ptr(), rhs._str.ptr()) != 0) {
	if (rhs.length() + 1 > size()) {
	    char *oldPtr = _str.resizeNoCopy(rhs.length() + 1);
	    delete [] oldPtr;
	}
	
	strcpy(_str.ptr(), rhs._str.ptr());
	_len = rhs.length();
    }
    return *this;
}

PMC_Bool 
PMC_String::operator==(const PMC_String &rhs) const
{
    if (_len == rhs._len && strcmp(ptr(), rhs.ptr()) == 0)
	return PMC_true;
    return PMC_false;
}

PMC_Bool
PMC_String::operator==(const char *rhs) const
{
    if (rhs == NULL) {
	if (_len == 0)
	    return PMC_true;
    }
    else if (strcmp(ptr(), rhs) == 0)
	return PMC_true;
    return PMC_false;
}
    
PMC_Bool
PMC_String::operator!=(const PMC_String &rhs) const
{
    if (_len != rhs._len && strcmp(ptr(), rhs.ptr()) != 0)
	return PMC_true;
    return PMC_false;
}

PMC_Bool
PMC_String::operator!=(const char *rhs) const
{
    if (rhs == NULL) {
	if (_len != 0)
	    return PMC_true;
    }
    else if (strcmp(ptr(), rhs) != 0)
	return PMC_true;
    return PMC_false;
}

void
PMC_String::push(uint_t len)
{
    char	*oldPtr;
    PMC_Bool	deleteFlag = PMC_false;
    uint_t	newLen = length() + len;
    uint_t	newSize;
    uint_t	i;

    if (newLen + 1 >= size()) {
	if (length() > len)
	    newSize = length() * 2;
	else
	    newSize = newLen + 1;
	oldPtr = _str.resizeNoCopy(newSize);
	deleteFlag = PMC_true;
    }
    else
	oldPtr = _str.ptr();

    for (i = newLen; i >= len; i--)
	_str.ptr()[i] = oldPtr[i - len];

    if (deleteFlag)
	delete [] oldPtr;

    _len = newLen;

}

PMC_String&
PMC_String::prepend(char const* str)
{
    uint_t i;
    if (str == NULL)
	return *this;

    uint_t len = strlen(str);
    push(len);
    for (i = 0; i < len; i++)
	_str.ptr()[i] = str[i];
    return *this;
}

PMC_String&
PMC_String::prepend(PMC_String const& str)
{
    uint_t i;
    if (str.length() == 0)
	return *this;

    push(str.length());
    for (i = 0; i < str.length(); i++)
	_str.ptr()[i] = str.ptr()[i];

    return *this;
}

void
PMC_String::extend(uint_t len)
{
    uint_t newLen = length() + len;
    uint_t newSize = length();

    if (newLen + 1 >= size()) {
	if (length() > len)
	    newSize += length();
	else
	    newSize += len + 1;
	_str.resize(newSize);
    }
    _str[newLen] = '\0';
    _len = newLen;
}

PMC_String&
PMC_String::append(char const* ptr)
{
    if (ptr == NULL)
    	return *this;

    uint_t i;
    uint_t len = strlen(ptr);
    uint_t oldLen = length();

    extend(len);
    for (i = 0; i < len; i++)
	_str.ptr()[i + oldLen] = ptr[i];
    return *this;
}

PMC_String&
PMC_String::append(PMC_String const& str)
{
    uint_t i;
    uint_t oldLen = length();
    
    extend(str.length());
    for (i = 0; i < str.length(); i++)
	_str.ptr()[i + oldLen] = str.ptr()[i];
    return *this;
}

PMC_String&
PMC_String::appendInt(int value, uint_t width)
{
    char buf[32];

    sprintf(buf, "%*d", width, value);
    return append(buf);
}

PMC_String&
PMC_String::appendReal(double value, uint_t precision)
{
    char buf[32];

    sprintf(buf, "%.*f", precision, value);
    return append(buf);
}

PMC_String& 
PMC_String::truncate(uint_t len)
{ 
    assert(len <= _len); 
    _len = len; \
    _str[len] = '\0'; 
    return *this;
}

PMC_String
PMC_String::substr(uint_t pos, uint_t len) const
{
    uint_t i;

    assert(pos + len < length());
    PMC_String newStr(len + 1);
    for (i = 0; i < len; i++)
	newStr._str.ptr()[i] = _str[i + pos];
    newStr._len = len;
    newStr._str.last() = '\0'; 
    return newStr;
}

PMC_String&
PMC_String::remove(uint_t pos, uint_t len)
{
    uint_t i;
    uint_t last = pos + len;

    assert(last <= length());
    for (i = last; i < length(); i++)
	_str.ptr()[i - len] = _str.ptr()[i];
    _len -= len;
    _str[length()] = '\0';
    return *this;
}

PMC_String&
PMC_String::resize(uint_t size)
{
    _str.resize(size);
    if (size <= length()) {
	_len = size - 1;
	_str[_len] = '\0';
    }
    return *this;
}

ostream&
operator<<(ostream &os, PMC_String const& rhs)
{
    os << rhs._str.ptr();
    return os;
}
