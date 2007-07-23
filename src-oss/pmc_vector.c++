/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: pmc_vector.c++,v 1.1 2005/05/26 06:38:49 kenmcd Exp $"

//
// Test PMC_Vector
//

#include <stdlib.h>
#include <iostream.h>
#include "Vector.h"

extern char *pmProgname;

void
dump(const PMC_IntVector &v)
{
    uint_t i;
    cout << '[';
    if (v.length() > 0)
	cout << v[0];
    for (i = 1; i < v.length(); i++)
	cout << ", " << v[i];
    cout << ']' << endl << endl;
}

int
main(int argc, char *argv[])
{
    uint_t i;

    pmProgname = basename(argv[0]);
    if (argc != 1) {
	cerr << "Usage: " << pmProgname << endl;
	exit(1);
	/*NOTREACHED*/
    }

// Test constructors

    PMC_IntVector v1;
    for (i = 0; i < v1.length(); i++)
	v1[i] = i;
    cout << "PMC_IntVector v1();" << endl;
    dump(v1);

    PMC_IntVector v2(5);
    for (i = 0; i < v2.length(); i++)
	v2[i] = i;
    cout << "PMC_IntVector v2(5);" << endl;
    dump(v2);

    PMC_IntVector v3(3, 3);
    cout << "PMC_IntVector v3(3, 3);" << endl;
    dump(v3);

    int numbers[4] = { 2, 4, 6, 8};
    PMC_IntVector v4(4, numbers);
    cout << "PMC_IntVector v4(4, numbers);" << endl;
    dump(v4);

    PMC_IntVector v5(v3);
    cout << "PMC_IntVector v5(v3);" << endl;
    dump(v5);

    PMC_IntVector v6 = v3;
    cout << "PMC_IntVector v6 = v3;" << endl;
    dump(v6);

    PMC_IntVector v7(3);
    v7 = v4;
    cout << "PMC_IntVector v7(3); v7 = v4;" << endl;
    dump(v7);


// operator[]

    v7[2] = 1;
    cout << "v7[2] = 1;" << endl;
    dump(v7);

// last

    v7.last() = 2;
    cout << "v7.last() = 2;" << endl;
    dump(v7);

// resize

    v2.resize(2);
    cout << "v2.resize(2);" << endl;
    dump(v2);

    v6.resize(6, 6);
    cout << "v6.resize(6, 6);" << endl;
    dump(v6);

// memory management

    PMC_IntVector v8(1024);
    v8.resize(2048);
    PMC_IntVector *v9 = new PMC_IntVector(v8);
    v8.resize(42);
    delete v9;

    return 0;
}
