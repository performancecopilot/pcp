/* -*- C++ -*- */

#ifndef _PMC_HASH_H_
#define _PMC_HASH_H_

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

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <sys/types.h>

template<class T>
class PMC_Hash
{
private:

    __pmHashCtl	*_table;

public:

    // Destroy the hash table and all items in it
    ~PMC_Hash();

    // Create an empty hash table
    PMC_Hash();

    // Add copy of <item> into hash table
    int add(int key, T const& item);

    // Add <item> into hash table, ~PMC_Hash() will destroy it.
    int add(int key, T* item);

    // Add copy <item> into hash table
    int add_cp(int key, T item)
	{ return add(key, item); }

    // Search for item with <key> 
    T *search(int key);

private:

    PMC_Hash(PMC_Hash const&)
	{}
    PMC_Hash const& operator=(PMC_Hash const&)
	{ return *this; }
};

typedef PMC_Hash<int> PMC_IntHash;

template<class T>
PMC_Hash<T>::~PMC_Hash()
{
    __pmHashNode *node;
    __pmHashNode *next;
    uint_t	i;

    for (i = 0; i < _table->hsize; i++) {
	node = _table->hash[i];
	while (node != NULL) {
	    next = node->next;
	    delete node->data;
	    free(node);
	    node = next;
	}
    }
    free(_table->hash);
    delete _table;
}

template<class T>
PMC_Hash<T>::PMC_Hash()
: _table(new __pmHashCtl)
{
    memset(_table, 0, sizeof(__pmHashCtl));
}

template<class T>
int
PMC_Hash<T>::add(int key, T const& item)
{
    T *ptr = new T(item);
    return __pmHashAdd(key, ptr, _table);
}

template<class T>
int
PMC_Hash<T>::add(int key, T* item)
{
    return __pmHashAdd(key, item, _table);
}

template<class T>
T*
PMC_Hash<T>::search(int key)
{
    __pmHashNode	*node = __pmHashSearch(key, _table);

    if (node == NULL)
	return NULL;
    else
	return (T *)node->data;
}

#endif /* _PMC_HASH_H_ */
