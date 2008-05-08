/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: pmc_list.c++,v 1.1 2005/05/26 06:38:49 kenmcd Exp $"

//
// Test PMC_List
//

#include <stdlib.h>
#include "List.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <strings.h>

extern char *pmProgname;

void
dump(const PMC_IntList &l)
{
    uint_t i;
    cout << "Size = " << l.size() << ", Length = " << l.length() << endl;
    cout << '[';
    if (l.length() > 0)
	cout << l[0];
    for (i = 1; i < l.length(); i++)
	cout << ", " << l[i];
    cout << ']' << endl << endl;
}

int
main(int argc, char *argv[])
{
    pmProgname = basename(argv[0]);
    if (argc != 1) {
	cerr << "Usage: " << pmProgname << endl;
	exit(1);
	/*NOTREACHED*/
    }

// Test constructors

    PMC_IntList l1;
    cout << "PMC_IntList l1;" << endl;
    dump(l1);

    PMC_IntList l2(5);
    cout << "PMC_IntList l2(5);" << endl;
    dump(l2);

    PMC_IntList l3(4, 4);
    cout << "PMC_IntList l3(4,4);" << endl;
    dump(l3);

    int numbers[5] = {2, 4, 6, 8, 10};
    PMC_IntList l4(5, numbers);
    cout << "PMC_IntList l4(5, numbers);" << endl;
    dump(l4);

    PMC_IntList l5(7, 5, numbers);
    cout << "PMC_IntList l5(7, 5, numbers);" << endl;
    dump(l5);

    PMC_IntList l6(l3);
    cout << "PMC_IntList l6(l3);" << endl;
    dump(l6);

    PMC_IntList l7 = l5;
    cout << "PMC_IntList l7 = l5;" << endl;
    dump(l7);

    l2 = l7;
    cout << "l2 = l7;" << endl;
    dump(l2);

// operator []

    l2[1] = 3;
    cout << "l2[1] = 3;" << endl;
    dump(l2);

// head

    l7.head() = 3;
    cout << "l7.head() = 3;" << endl;
    dump(l7);

// tail

    l7.tail() = 12;
    cout << "l7.tail() = 12;" << endl;
    dump(l7);

// insert

    l7.insert(7);
    cout << "l7.insert(7);" << endl;
    dump(l7);

    l7.insert(9, 3);
    cout << "l7.insert(9, 3);" << endl;
    dump(l7);

    l7.insert(15, 6);
    cout << "l7.insert(15, 6);" << endl;
    dump(l7);

    l7.insert(5, numbers, 2);
    cout << "l7.insert(5, numbers, 2);" << endl;
    dump(l7);

    l7.insert(2, numbers, 10);
    cout << "l7.insert(2, numbers, 10);" << endl;
    dump(l7);

    l4.insert(l2);
    cout << "l4.insert(l2);" << endl;
    dump(l4);

    l4.insert(l1);
    cout << "l4.insert(l1);" << endl;
    dump(l4);

// append

    l4.append(3);
    cout << "l4.append(3);" << endl;
    dump(l4);

    l4.append(4, numbers);
    cout << "l4.append(4, numbers);" << endl;
    dump(l4);

    l4.append(l1);
    cout << "l4.append(l1);" << endl;
    dump(l4);

    l3.append(l5);
    cout << "l3.append(l5);" << endl;
    dump(l3);

    l5.append(l2);
    cout << "l5.append(l2);" << endl;
    dump(l5);

// remove

    // remove 1st 5 elements
    cout << "l7.remove(0, 5);" << endl;
    cout << "Before:" << endl;
    dump(l7);
    l7.remove(0, 5);
    cout << "After:" << endl;
    cout << "l7.remove(0, 5);" << endl;
    dump(l7);

    // remove last 4 elements 
    // ensure vector size is the same as the list length
    int more_numbers[15] = {2, 3, 6, 8, 10, 2, 4, 6, 8, 10, 3, 2, 4, 6, 8};
    PMC_IntList l8(15, more_numbers);
    cout << "l8.remove(l8.length() - 4, 4);" << endl;
    cout << "Before:" << endl;
    dump(l8);
    l8.remove(l8.length() - 4, 4);
    cout << "After:" << endl;
    dump(l8);
    

    // remove last 4 elements 
    cout << "l4.remove(l4.length() - 4, 4);" << endl;
    cout << "Before:" << endl;
    dump(l4);
    l4.remove(l4.length() - 4, 4);
    cout << "After:" << endl;
    dump(l4);

    cout << "l3.removeAll();" << endl;
    cout << "Before:" << endl;
    dump(l3);
    l3.removeAll();
    cout << "After:" << endl;
    dump(l3);

// destroy

    l4.destroy(3, 5);
    cout << "l4.destroy(3, 5);" << endl;
    dump(l4);

    l4.destroyAll(2);
    cout << "l4.destroyAll();" << endl;
    dump(l4);

// resize

    l7.resize(l7.length() + 2);
    cout << "l7.resize(l&.length() + 2);" << endl;
    dump(l7);

    l7.sync();
    cout << "l7.sync();" << endl;
    dump(l7);

    l7.resize(l7.length() - 2);
    cout << "l7.resize(l&.length() - 2);" << endl;
    dump(l7);
    
// additional bounds checks

    l4.insert(5, numbers);
    cout << "l4.insert(5, numbers);" << endl;
    dump(l4);

    l3.append(l7);
    cout << "l3.append(l7);" << endl;
    dump(l3);

    return 0;
}
