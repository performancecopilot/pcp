/* -*- C++ -*- 
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef _PMC_VECTOR_H_
#define _PMC_VECTOR_H_

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <assert.h>
#include <pcp/platform_defs.h>

template<class T>
class PMC_Vector
{
private:

    T		*_ptr;
    uint_t	_len;

public:

    ~PMC_Vector();

    // Constructors
    PMC_Vector(uint_t len = 4);
    PMC_Vector(uint_t len, T item);
    PMC_Vector(uint_t len, T const* ptr);
    PMC_Vector(PMC_Vector<T> const& rhs);

    // Copy assignment
    PMC_Vector<T> const& operator=(PMC_Vector<T> const& rhs);

    // Items in vector
    uint_t length() const
	{ return _len; }

    // Access buffer
    T const* ptr() const
	{ return _ptr; }
    T* ptr()
	{ return _ptr; }

    // Access elements
    T const& operator[](uint_t pos) const
	{ assert(pos < _len); return _ptr[pos]; }
    T& operator[](uint_t pos)
	{ assert(pos < _len); return _ptr[pos]; }

    // Access last element
    T const& last() const
	{ return _ptr[_len-1]; }
    T& last()
	{ return _ptr[_len-1]; }

    // Resize and copy old vector into new vector
    void resize(uint_t len);
    void resize(uint_t len, T const& item);
    void resizeCopy(uint_t len, T item)
	{ resize(len, item); }

    // Resize vector but do not copy or destroy old vector
    // The old vector is returned, called should delete [] it
    T* resizeNoCopy(uint_t len);
};

typedef PMC_Vector<int> PMC_IntVector;
typedef PMC_Vector<double> PMC_RealVector;


template<class T>
PMC_Vector<T>::~PMC_Vector()
{
    delete [] _ptr;
}

template<class T>
PMC_Vector<T>::PMC_Vector(uint_t len)
: _ptr(0), _len(len)
{
    assert(_len > 0);
    _ptr = new T[_len];
}

template<class T>
PMC_Vector<T>::PMC_Vector(uint_t len, T item)
: _ptr(0), _len(len)
{
    uint_t i;

    assert(_len > 0);
    _ptr = new T[_len];
    for (i = 0; i < _len; i++)
	_ptr[i] = item;
}

template<class T>
PMC_Vector<T>::PMC_Vector(uint_t len, T const* ptr)
: _ptr(0), _len(len)
{
    uint_t i;

    assert(_len > 0);
    _ptr = new T[_len];
    for (i = 0; i < _len; i++)
	_ptr[i] = ptr[i];
}

template<class T>
PMC_Vector<T>::PMC_Vector(PMC_Vector<T> const& rhs)
: _ptr(0), _len(rhs._len)
{
    uint_t i;

    _ptr = new T[_len];
    for (i = 0; i < _len; i++)
	_ptr[i] = rhs._ptr[i];
}

template<class T>
PMC_Vector<T> const&
PMC_Vector<T>::operator=(PMC_Vector<T> const& rhs)
{
    uint_t i;

    if (this != &rhs) {
	if (_len != rhs._len) {
	    delete [] _ptr;
	    _len = rhs._len;
	    _ptr = new T[_len];
	}
	for (i = 0; i < _len; i++)
	    _ptr[i] = rhs._ptr[i];
    }
    return *this;
}

template<class T>
void
PMC_Vector<T>::resize(uint_t len)
{
    uint_t i;

    assert(len > 0);
    if (_len != len) {
	T* oldPtr = _ptr;
	_ptr = new T[len];
	for (i = 0; i < len && i < _len; i++)
	    _ptr[i] = oldPtr[i];
	delete [] oldPtr;
	_len = len;
    }
}

template<class T>
void
PMC_Vector<T>::resize(uint_t len, T const& item)
{
    uint_t i;
    uint_t oldLen = _len;

    resize(len);
    for (i = oldLen; i < _len; i++)
	_ptr[i] = item;
}

template<class T>
T*
PMC_Vector<T>::resizeNoCopy(uint_t len)
{
    assert(len > 0);

    T* oldPtr = _ptr;
    _ptr = new T[len];
    _len = len;
    return oldPtr;
}

#endif /* _PMC_VECTOR_H_ */
