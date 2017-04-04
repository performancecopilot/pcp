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
//  TODO.C - COM functions required by MLANLL.C, MLANTRNU, MLANNETU.C and
//           MLanFile.C for MLANU to communicate with the DS2480 based 
//           Universal Serial Adapter 'U'.  Fill in the platform specific code.
//
//  Version: 1.02
//
//  History: 1.00 -> 1.01  Added function msDelay. 
//
//           1.01 -> 1.02  Changed to generic OpenCOM/CloseCOM for easier 
//                         use with other platforms.
//

//--------------------------------------------------------------------------
// Copyright (C) 1998 Andrea Chambers and University of Newcastle upon Tyne,
// All Rights Reserved.
//--------------------------------------------------------------------------
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
// IN NO EVENT SHALL THE UNIVERSITY OF NEWCASTLE UPON TYNE OR ANDREA CHAMBERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
// THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//---------------------------------------------------------------------------
//
//  LinuxLNK.C - COM functions required by MLANLLU.C, MLANTRNU.C, MLANNETU.C 
//             and MLanFile.C for MLANU to communicate with the DS2480 based
//             Universal Serial Adapter 'U'.  Platform specific code.
//
//  Version: 1.03
//  History: 1.00 -> 1.03 modifications by David Smiczek
//                        Changed to use generic OpenCOM/CloseCOM  
//                        Pass port name to OpenCOM instead of hard coded
//                        Changed msDelay to handle long delays 
//                        Reformatted to look like 'TODO.C' 
//                        Added #include "ds2480.h" to use constants.
//                        Added function SetBaudCOM() 
//                        Added function msGettick()
//                        Removed delay from WriteCOM(), used tcdrain()
//                        Added wait for byte available with timeout using
//                          select() in ReadCOM()
/* 
   cfmakeraw function from nut-0.45.0 package
   common.c - common useful functions

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <termios.h> 
#include <sys/time.h>

#include "mlan.h"
#include "ds2480.h"
#include "pmapi.h"

// Exportable functions required for MLANLL.C, MLANTRNU.C, or MLANNETU.C
void FlushCOM(void);
int  WriteCOM(int, unsigned char*);
int  ReadCOM(int, unsigned char*);
void BreakCOM(void);
void msDelay(int);
long msGettick(void);

// Exportable functions for opening/closing serial port
int OpenCOM(char *);
void CloseCOM(void);

// LinuxLNK global
int fd;

#ifdef IS_SOLARIS
int cfmakeraw(struct termios *termios_p)
{
  termios_p->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
			  |INLCR|IGNCR|ICRNL|IXON);
  termios_p->c_oflag &= ~OPOST;
  termios_p->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
  termios_p->c_cflag &= ~(CSIZE|PARENB);
  termios_p->c_cflag |= CS8;
  return 0;
}
#endif

//--------------------------------------------------------------------------
// Write an array of bytes to the COM port, verify that it was
// sent out.  Assume that baud rate has been set.
//
// Returns 1 for success and 0 for failure
//   
int WriteCOM(int outlen, uchar *outbuf)
{
   long count = outlen;
   int i;
   int	sts;

   if (MLanDebug) {
       fprintf(stderr, "WriteCOM: calling write: %d bytes:", outlen);
       for (i = 0; i < outlen; i++) fprintf(stderr, " %02x", 0xff & outbuf[i]);
       fputc('\n', stderr);
       fflush(stderr);
   }

   i = write(fd, outbuf, outlen);

   if (MLanDebug) {
       fprintf(stderr, "WriteCOM: write returns %d\nWriteCOM: calling tcdrain\n", i);
       fflush(stderr);
   }

   sts = tcdrain(fd);        

   if (MLanDebug) {
       fprintf(stderr, "WriteCOM: tcdrain returns %d\n", sts);
       fflush(stderr);
   }

   return (i == count);
}  


//--------------------------------------------------------------------------
// Read an array of bytes to the COM port, verify that it was
// sent out.  Assume that baud rate has been set.
//
// Returns number of characters read
//
int ReadCOM(int inlen, uchar *inbuf)
{  
   fd_set         filedescr;
   struct timeval tval;
   int            cnt; 
   int		  sts;

   if (MLanDebug) {
       fprintf(stderr, "ReadCOM: calling read: want %d bytes:", inlen);
       fflush(stderr);
   }

   // loop to wait until each byte is available and read it
   for (cnt = 0; cnt < inlen; cnt++)
   {
      // set a descriptor to wait for a character available
      FD_ZERO(&filedescr);
      FD_SET(fd,&filedescr);
      // set timeout to 10ms  
      tval.tv_sec = 0;
      tval.tv_usec = 10000;

      // if byte available read or return bytes read 
      if (0 != select(fd+1,&filedescr,NULL,NULL,&tval)) {
         if ((sts = read(fd,&inbuf[cnt],1)) != 1) {
	    if (MLanDebug) {
	       fprintf(stderr, ": read returns %d, got %d bytes\n", sts, cnt);
	       fflush(stderr);
	    }
            return cnt;
	 }
         if (MLanDebug) {
	    fprintf(stderr, " %02x", 0xff & inbuf[cnt]);
	    fflush(stderr);
	 }
      }
      else {
	 if (MLanDebug) {
	    fprintf(stderr, ": select timeout, got %d bytes\n", cnt);
	    fflush(stderr);
	 }
         return cnt;
      }
      
   }
   
   // success, so return desired length
   if (MLanDebug) {
      fprintf(stderr, ": got 'em all\n");
      fflush(stderr);
   }
   return inlen;
}


//---------------------------------------------------------------------------
//  Description:
//     flush the rx and tx buffers
//
void FlushCOM(void)    
{    
   tcflush (fd, TCIOFLUSH);  
}  


//--------------------------------------------------------------------------
//  Description:
//     Delay for at least 'len' ms
// 
void msDelay(int len)
{
   struct timespec s;              // Set aside memory space on the stack      

   s.tv_sec = len / 1000;
   s.tv_nsec = (len - (s.tv_sec * 1000)) * 1000000;   
   nanosleep(&s, NULL);
}


//--------------------------------------------------------------------------
//  Description:
//     Send a break on the com port for at least 2 ms
// 
void BreakCOM(void)      
{
   int duration = 0;              // see man termios break may be 
   tcsendbreak(fd, duration);     // too long       
}


//---------------------------------------------------------------------------
// Attempt to open a com port.  
// Set the starting baud rate to 9600.
//
// 'port_zstr' - zero terminate port name.  Format is platform
//               dependent.
//
// Returns: TRUE - success, COM port opened
//
int OpenCOM(char *port_zstr)
{     
   struct termios t;               // see man termios - declared as above 
   int rc;

   fd = open(port_zstr, O_RDWR|O_NONBLOCK); 
   if (fd<0) return fd;
   rc = tcgetattr (fd, &t);
   if (rc < 0)
   {
      int tmp;
      tmp = oserror();
      close(fd);
      setoserror(tmp);
      return rc;
   }
   if (MLanDebug) {
       fprintf(stderr, "OpenCOM: initial tty settings\niflag: %07o oflag: %07o lflag: %07o cflag: %07o\n", (unsigned int)t.c_iflag, (unsigned int)t.c_oflag, (unsigned int)t.c_lflag, (unsigned int)t.c_cflag);
       fflush(stderr);
   }

   cfsetospeed(&t, B9600);
   cfsetispeed (&t, B9600);

#ifdef IRIX
// IRIX tty games ...
//
// per the Linux man page cfmakeraw sets the terminal attributes as follows:
//
   t.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
   t.c_iflag &= ~(ISTRIP|INLCR|IGNCR|ICRNL|IXON);
   t.c_oflag &= ~OPOST;
   t.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
   t.c_cflag &= ~(CSIZE|PARENB);
   t.c_cflag |= CS8;
// to this we have to clear the additional IRIX goodies like:
//
   t.c_iflag &= ~(IXANY|IGNPAR);
   t.c_oflag &= ~(TAB3);
// and finally, CLOCAL is in c_cflag, not c_lflag
//
   t.c_cflag |= CLOCAL;            // ignore modem signals
#else
// assumed to be Linux
//
   cfmakeraw(&t);                  // don't generate signals, translate or echo
   t.c_lflag |= CLOCAL;            // ignore modem signals
#endif

   if (MLanDebug) {
       fprintf(stderr, "OpenCOM: new tty settings\niflag: %07o oflag: %07o lflag: %07o cflag: %07o\n", (unsigned int)t.c_iflag, (unsigned int)t.c_oflag, (unsigned int)t.c_lflag, (unsigned int)t.c_cflag);
       fprintf(stderr, "OpenCOM: calling tcsetattr\n");
       fflush(stderr);
   }
   rc = tcsetattr (fd, TCSAFLUSH, &t);
   if (MLanDebug) {
       fprintf(stderr, "OpenCOM: tcsetattr returns %d\n", rc);
       fflush(stderr);
   }
   if (rc < 0)
   {
      int tmp;
      tmp = oserror();
      close(fd);
      setoserror(tmp);
      return rc;
   }

   return fd; 
}


//---------------------------------------------------------------------------
// Closes the connection to the port.
//
void CloseCOM(void)
{
   FlushCOM();
   close(fd);
}


//--------------------------------------------------------------------------
// Set the baud rate on the com port.  The possible baud rates for
// 'new_baud' are:
//
// PARMSET_9600     0x00
// PARMSET_19200    0x02
// PARMSET_57600    0x04
// PARMSET_115200   0x06
// 
void SetBaudCOM(int new_baud)
{
   struct termios t;               
   int rc;
   speed_t baud = B0;

   // read the attribute structure
   rc = tcgetattr(fd, &t);
   if (rc < 0)
   {
      close(fd);
      return;
   }

   // convert parameter to linux baud rate
   switch(new_baud)
   {
      case PARMSET_9600:
         baud = B9600;
         break;
      case PARMSET_19200:
         baud = B19200;
         break;
      case PARMSET_57600:
#ifdef B57600
         baud = B57600;
         break;
#else
#define ERR_MSG_57600 "SetBaudCOM: no support for 57600 baud, sorry!"
	 write(2, ERR_MSG_57600, strlen(ERR_MSG_57600));
	 exit(1);
#endif
      case PARMSET_115200:
#ifdef B115200
         baud = B115200;
         break;
#else
#define ERR_MSG_115200 "SetBaudCOM: no support for 115200 baud, sorry!"
	 write(2, ERR_MSG_115200, strlen(ERR_MSG_115200));
	 exit(1);
#endif
   }

   // set baud in structure 
   cfsetospeed(&t, baud);
   cfsetispeed(&t, baud);
  
   // change baud on port 
   rc = tcsetattr(fd, TCSAFLUSH, &t);
   if (rc < 0)
      close(fd);
}


//--------------------------------------------------------------------------
// Get the current millisecond tick count.  Does not have to represent
// an actual time, it just needs to be an incrementing timer.
//
long msGettick(void)
{
   struct timezone tmzone;
   struct timeval  tmval;
   long ms; 

   gettimeofday(&tmval,&tmzone);
   ms = (tmval.tv_sec & 0xFFFF) * 1000 + tmval.tv_usec / 1000;   
   return ms;
}

