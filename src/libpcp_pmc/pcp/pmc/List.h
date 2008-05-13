/* -*- C++ -*- */

#ifndef _PMC_LIST_H_
#define _PMC_LIST_H_

/*
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
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <assert.h>
#include <pcp/pmc/Vector.h>
#include <pcp/pmc/Bool.h>

template<class T>
class PMC_List
{
private:

    PMC_Vector<T>	_list;
    uint_t		_len;

public:

    ~PMC_List();

    // Constructors
    PMC_List(uint_t size = 4)
	: _list(size), _len(0) {}
    PMC_List(uint_t len, T item)
	: _list(len, item), _len(len) {}
    PMC_List(uint_t len, T const* ptr)
	: _list(len, ptr), _len(len) {}
    PMC_List(uint_t newSize, uint_t len, T const* ptr);
    PMC_List(PMC_List<T> const& rhs)
	: _list(rhs._list), _len(rhs._len) {}

    // Copy assignment
    PMC_List<T> const& operator=(PMC_List<T> const& rhs);

    // Number of items in list
    uint_t length() const
	{ return _len; }

    // Size of buffer
    uint_t size() const
	{ return _list.length(); }

    // Access to the buffer
    T const* ptr() const
	{ return _list.ptr(); }
    T* ptr()
	{ return _list.ptr(); }

    // Access to elements
    T const& operator[](uint_t pos) const
	{ assert(pos < _len); return _list[pos]; }
    T& operator[](uint_t pos)
	{ assert(pos < _len); return _list[pos]; }

    // Access to first element
    T const& head() const
	{ assert(_len > 0); return _list[0]; }
    T& head()
	{ assert(_len > 0); return _list[0]; }

    // Access to last element
    T const& tail() const
	{ assert(_len > 0); return _list[_len - 1]; }
    T& tail()
	{ assert(_len > 0); return _list[_len - 1]; }

    // Insert an item at <pos>
    PMC_List<T>& insert(T const& item, uint_t pos = 0);
    PMC_List<T>& insert(uint_t len, T const* ptr, uint_t pos = 0);
    PMC_List<T>& insert(PMC_List<T> const& list, uint_t pos = 0)
	{ return insert(list.length(), list.ptr(), pos); }
    PMC_List<T>& insertCopy(T item, uint_t pos = 0)
	{ return insert(item, pos); }

    // Append item to list
    PMC_List<T>& append(T const& item);
    PMC_List<T>& append(uint_t len, T const* ptr);
    PMC_List<T>& append(PMC_List<T> const& list)
	{ return append(list.length(), list.ptr()); }
    PMC_List<T>& appendCopy(T item)
	{ return append(item); }

    // Remove <len> items starting at <pos>. Items may not be deleted
    // but may be replaced later
    PMC_List<T>& remove(uint_t pos, uint_t len = 1);

    // Remove all items from the list
    PMC_List<T>& removeAll()
	{ _len = 0; return *this; }

    // Remove <len> items starting at <pos>. Items are deleted by
    // copying remaining items to a new buffer and deleting the old
    // buffer
    PMC_List<T>& destroy(uint_t pos, uint_t len = 1);

    // Destroy existing buffer and all items in it
    PMC_List<T>& destroyAll(uint_t newSize = 4);

    // Resize list to <size>, deleting items beyond <size>
    PMC_List<T>& resize(uint_t size);

    // Resize to current list length
    void sync()
	{ _list.resize(_len); }

private:

    void push(uint_t pos, uint_t len);

};

typedef PMC_List<int> PMC_IntList;
typedef PMC_List<double> PMC_RealList;

template<class T>
PMC_List<T>::~PMC_List()
{
}

template<class T>
PMC_List<T>::PMC_List(uint_t newSize, uint_t len, T const* ptr)
: _list(newSize), _len(len)
{
    uint_t i;

    assert(size() >= length());
    for (i = 0; i < length(); i++)
	_list[i] = ptr[i];
}

template<class T>
PMC_List<T> const& 
PMC_List<T>::operator=(PMC_List<T> const& rhs)
{
    if (this != &rhs) {
	_list = rhs._list;
	_len = rhs._len;
    }
    return *this;
}

template<class T>
void
PMC_List<T>::push(uint_t pos, uint_t len)
{
    T		*oldPtr;
    PMC_Bool	deleteFlag = PMC_false;
    uint_t	newLen = length() + len;
    uint_t	newSize = length();
    uint_t	last = pos + len;
    uint_t	i;

    assert(pos <= length());

    if (newLen > size()) {
	if (length() > len)
	    newSize += length();
	else
	    newSize += len;
	oldPtr = _list.resizeNoCopy(newSize);
	for (i = 0; i < pos; i++)
	    _list.ptr()[i] = oldPtr[i];
	deleteFlag = PMC_true;
    }
    else
	oldPtr = _list.ptr();
    
    for (i = newLen - 1; i >= last; i--)
	_list.ptr()[i] = oldPtr[i - len];

    if (deleteFlag) 
	delete [] oldPtr;

    _len = newLen;
}

template<class T>
PMC_List<T>& 
PMC_List<T>::insert(T const& item, uint_t pos)
{
    push(pos, 1);
    _list[pos] = item;
    return *this;
}

template<class T>
PMC_List<T>& 
PMC_List<T>::insert(uint_t len, T const* ptr, uint_t pos)
{
    uint_t i;

    assert(_list.ptr() != ptr);

    if (len == 0)
	return *this;

    push(pos, len);
    for (i = 0; i < len; i++)
	_list[i + pos] = ptr[i];
    return *this;
}

template<class T>
PMC_List<T>& 
PMC_List<T>::append(T const& item)
{
    if (length() >= size())
	_list.resize(length() * 2);
    _list[_len++] = item;
    return *this;
}

template<class T>
PMC_List<T>& 
PMC_List<T>::append(uint_t len, T const* ptr)
{
    uint_t newLen = length() + len;
    uint_t oldLen = length();
    uint_t newSize;
    uint_t i;

    if (len == 0)
	return *this;

    if (newLen > size()) {
	if (length() > len)
	    newSize = length() * 2;
	else
	    newSize = newLen;
	_list.resize(newSize);
    }

    for (i = 0; i < len; i++)
	_list[i + oldLen] = ptr[i];
    _len = newLen;

    return *this;
}

template<class T>
PMC_List<T>& 
PMC_List<T>::remove(uint_t pos, uint_t len)
{
    uint_t i;
    uint_t last = _len - len;

    assert(last <= length());

    for (i = pos; i < last; i++)
	_list[i] = _list[i + len];

    _len -= len;

    return *this;
}

template<class T>
PMC_List<T>& 
PMC_List<T>::destroy(uint_t pos, uint_t len)
{
    uint_t i;
    uint_t last = pos + len;

    assert(last <= length());

    T* oldPtr = _list.resizeNoCopy(length() - len);

    for (i = 0; i < pos; i++)
	_list[i] = oldPtr[i];
    for (i = last; i < length(); i++)
	_list[i - len] = oldPtr[i];

    _len -= len;
    
    delete [] oldPtr;

    return *this;
}

template<class T>
PMC_List<T>& 
PMC_List<T>::destroyAll(uint newSize)
{
    T* oldPtr = _list.resizeNoCopy(newSize);
    delete [] oldPtr;
    _len = 0;
    return *this;
}

template<class T>
PMC_List<T>&
PMC_List<T>::resize(uint_t newSize)
{
    _list.resize(newSize);
    if (size() < length())
	_len = size();
    return *this;
}

#endif /* _PMC_LIST_H_ */
