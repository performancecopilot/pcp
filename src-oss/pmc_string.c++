/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: pmc_string.c++,v 1.1 2005/05/26 06:38:49 kenmcd Exp $"

//
// Test PMC_SAtring
//

#include <stdlib.h>
#include "String.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <strings.h>

extern char *pmProgname;

void
dump(const PMC_String &s)
{
    cout << "Size = " << s.size() << ", Length = " << s.length() 
	 << ": \"" << s << '"' << endl << endl;
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

    PMC_String s1;
    cout << "PMC_String s1;" << endl;
    dump(s1);

    PMC_String s2(12);
    cout << "PMC_String s2(12);" << endl;
    dump(s2);

    PMC_String s3((const char *)0);
    cout << "PMC_String s3((const char *)0);" << endl;
    dump(s3);

    PMC_String s4("Hello there");
    cout << "PMC_String s4(\"Hello there\");" << endl;
    dump(s4);

    PMC_String s5(s4);
    cout << "PMC_String s5(s4);" << endl;
    dump(s5);

    PMC_String s6 = s5;
    cout << "PMC_String s6 = s5;" << endl;
    dump(s6);

    PMC_String s7(4);
    s7 = s4;
    cout << "PMC_String s7(4); s7 = s4;" << endl;
    dump(s7);

    PMC_String s8(32);
    s8 = "Yes you bozo";
    cout << "PMC_String s8(32); s8 = \"Yes you bozo\";" << endl;
    dump(s8);

    s7 = (char *)0;
    cout << "s7 = (char *)0;" << endl;
    dump(s7);

// operator []

    s6[5] = '-';
    cout << "s6[5] = '-';" << endl;
    dump(s6);

// operator ==

    cout << "s4 == s5: " << (s4 == s5) << endl;
    cout << "s4 != s5: " << (s4 != s5) << endl;
    cout << "s4 == s8: " << (s4 == s8) << endl;
    cout << "s4 != s8: " << (s4 != s8) << endl;
    cout << "s4 == (const char *)0: " << (s4 == (const char *)0) << endl;
    cout << "s4 != (const char *)0: " << (s4 != (const char *)0) << endl;
    cout << endl;

// prepend

    s8.prepend('!');
    cout << "s8.prepend('!');" << endl;
    dump(s8);

    s5.prepend('!');
    cout << "s5.prepend('!');" << endl;
    dump(s5);

    s1.prepend('a');
    cout << "s1.prepend('a');" << endl;
    dump(s1);

    s5.prepend("just testing");
    cout << "s5.prepend(\"just testing\");" << endl;
    dump(s5);

    s5.prepend((const char *)0);
    cout << "s5.prepend((const char *)0);" << endl;
    dump(s5);

    s3.prepend(s5);
    cout << "s3.prepend(s5);" << endl;
    dump(s3);

// append

    s8.appendChar('!');
    cout << "s8.append('!');" << endl;
    dump(s8);

    s4.appendChar('!');
    cout << "s4.append('!');" << endl;
    dump(s4);

    s2.appendChar('a');
    cout << "s2.append('a');" << endl;
    dump(s2);

    s5.append(", just testing");
    cout << "s5.append(\", just testing\");" << endl;
    dump(s5);

    s5.append((const char *)0);
    cout << "s5.append((const char *)0);" << endl;
    dump(s5);

    s6.append(s2);
    cout << "s6.append(s2);" << endl;
    dump(s6);

    s2.appendInt(42, 10);
    cout << "s2.appendInt(42, 10);" << endl;
    dump(s2);

    s2.appendReal(3.14, 4);
    cout << "s2.appendReal(3.14, 4);" << endl;
    dump(s2);

// truncate

    s5.truncate(10);
    cout << "s5.truncate(10);" << endl;
    dump(s5);

    s6.truncate(12);
    cout << "s6.truncate(12);" << endl;
    dump(s6);

    s6.truncate(0);
    cout << "s6.truncate(0);" << endl;
    dump(s6);

// substr

    s4 = s3.substr(3, 7);
    cout << "s4 = s3.substr(3, 7);" << endl;
    dump(s4);

// remove

    s3.remove(3, 7);
    cout << "s3.remove(3, 7);" << endl;
    dump(s3);

    s3.remove(0, s3.length());
    cout << "s3.remove(0, s3.length());" << endl;
    dump(s3);

// resize

    s4.resize(s4.length() + 1);
    cout << "s4.resize(s4.length() + 1);" << endl;
    dump(s4);

    s4.resize(s4.length() * 2);
    cout << "s4.resize(s4.length() * 2);" << endl;
    dump(s4);

    s4.resize(s4.length());
    cout << "s4.resize(s4.length());" << endl;
    dump(s4);

    s4.resize(1);
    cout << "s4.resize(1);" << endl;
    dump(s4);

// sync

    s5.sync();
    cout << "s5.sync();" << endl;
    dump(s5);

    return 0;
}
