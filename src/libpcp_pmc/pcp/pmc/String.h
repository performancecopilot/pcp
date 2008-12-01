/* -*- C++ -*- */

#ifndef _PMC_STRING_H_
#define _PMC_STRING_H_

/*
 * Copyright (c) 1998,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <pcp/pmc/Vector.h>
#include <pcp/pmc/List.h>
#include <pcp/pmc/Bool.h>
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

typedef PMC_Vector<char> PMC_CharVector;

class PMC_String
{
private:

    uint_t		_len;
    PMC_CharVector	_str;

    void push(uint_t len);
    void extend(uint_t len);

public:

    ~PMC_String()
	{}

    // Constructors
    PMC_String()
	: _len(0), _str(16) { _str[0] = '\0'; }

    PMC_String(uint_t size)
	: _len(0), _str(size) { _str[0] = '\0'; }

    PMC_String(char const* str);

    PMC_String(PMC_String const& rhs)
	: _len(rhs._len), _str(rhs._str) {}

    PMC_String const& operator=(char const* ptr);
    PMC_String const& operator=(PMC_String const& rhs);

    // Length of the string, note it does not need to call strlen
    uint_t length() const
	{ return _len; }

    // Size of the buffer holding the string
    uint_t size() const
	{ return _str.length(); }
	    
    // A handle to the actual string
    const char* ptr() const
	{ return _str.ptr(); }
    char* ptr()
	{ return _str.ptr(); }

    // Access the characters in the string
    const char &operator[](uint_t pos) const
	{ assert(pos <= length()); return _str[pos]; }
    char &operator[](uint_t pos)
	{ assert(pos <= length()); return _str[pos]; }
    
    // Compare strings
    PMC_Bool operator==(const PMC_String &rhs) const;
    PMC_Bool operator==(const char *rhs) const;
    PMC_Bool operator!=(const PMC_String &rhs) const;
    PMC_Bool operator!=(const char *rhs) const;
    
    // Insert at the start of the string
    PMC_String& prepend(char item)
	{ push(1); _str[0] = item; return *this; }
    PMC_String& prepend(const char *ptr);
    PMC_String& prepend(const PMC_String& str);

    // Add to the end of the string
    PMC_String& append(const char *ptr);
    PMC_String& append(const PMC_String& str);

    // Cannot overload append method as results may be ambiguous
    PMC_String& appendChar(char item)
	{ _str[length()] = item; extend(1); return *this; }
    PMC_String& appendInt(int value, uint_t width = 1);
    PMC_String& appendReal(double value, uint_t precision = 3);

    // Truncate the string to <len> characters
    PMC_String& truncate(uint_t len);

    // Return a sub string starting at <pos> for <len> characters
    PMC_String substr(uint_t pos, uint_t len) const;

    // Remove a sub string starting at <pos> for <len> characters
    PMC_String& remove(uint_t pos, uint_t len);

    // Resize the buffer to <size>, truncating if needed
    PMC_String& resize(uint_t size);

    // Resize the buffer to the actual length of the string
    PMC_String& sync()
	{ _str.resize(_len + 1); return *this; }

    // Output the string
    friend ostream& operator<<(ostream &os, PMC_String const& rhs);
};

typedef PMC_List<PMC_String> PMC_StrList;

#endif /* _PMC_STRING_H_ */
