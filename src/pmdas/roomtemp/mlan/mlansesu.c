//---------------------------------------------------------------------------
// Copyright (C) 1999 Dallas Semiconductor Corporation, All Rights Reserved.
// 
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included 
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES 
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
// OTHER DEALINGS IN THE SOFTWARE.
// 
// Except as contained in this notice, the name of Dallas Semiconductor 
// shall not be used except as stated in the Dallas Semiconductor 
// Branding Policy. 
//---------------------------------------------------------------------------
//
//  MLanSesU.C - Aquire and release a Session on the 1-Wire Net.
//
//  Version: 1.03
//

#include "pmapi.h"
#include "mlan.h"

// external function prototypes
extern int  OpenCOM(char *);
extern void CloseCOM(void);
extern int  DS2480Detect(void);

// keep port name for later message when closing
char portname[128];

// debugging
int MLanDebug = 0;

//---------------------------------------------------------------------------
// Attempt to aquire a 1-Wire net using a com port and a DS2480 based
// adapter.  
//
// 'port_zstr'  - zero terminated port name.  For this platform
//                use format COMX where X is the port number.
// 'msg'        - zero terminated return message.
// 'len'        - length of the supplied return message buffer.
//
// Returns: TRUE - success, COM port opened
//
int Aquire1WireNet(char *port_zstr, char *msg, int len)
{
   int cnt = 0;
   portname[0] = 0;

   // attempt to open the communications port
   if (OpenCOM(port_zstr) >= 0)
      cnt += pmsprintf(&msg[cnt], len-cnt, "%s opened\n",port_zstr);
   else {
      cnt += pmsprintf(&msg[cnt], len-cnt, "Could not open port %s: %s,"
              " aborting.\nClosing port %s.\n",
	      port_zstr, osstrerror(), port_zstr);
      return FALSE;
   }

   // detect DS2480
   if (DS2480Detect())
      cnt += pmsprintf(&msg[cnt], len-cnt, "DS2480-based adapter detected\n");
   else {
      cnt += pmsprintf(&msg[cnt], len-cnt, "DS2480-based adapter not detected, aborting program\n");
      cnt += pmsprintf(&msg[cnt], len-cnt, "Closing port %s.\n", port_zstr);
      CloseCOM();
      return FALSE;
   }      

   // success
   pmsprintf(portname, sizeof(portname), "%s", port_zstr);
   return TRUE;
}


//---------------------------------------------------------------------------
// Release the previously aquired a 1-Wire net.
//
// 'msg'        - zero terminated return message.
// 'len'        - length of the supplied return message buffer.
//
void Release1WireNet(char *msg, int len)
{
   // close the communications port
   pmsprintf(msg, len, "Closing port %s.\n", portname);
   CloseCOM();
}
