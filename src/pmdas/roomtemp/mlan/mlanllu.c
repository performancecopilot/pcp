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
//  MLanLLU.C - Link Layer MicroLAN functions using the DS2480 (U)
//              serial interface chip.
//
//  Version: 1.03
//
//  History: 1.00 -> 1.01  DS2480 version number now ignored in 
//                         MLanTouchReset.
//           1.02 -> 1.03  Removed caps in #includes for Linux capatibility
//                         Removed #include <windows.h> 
//                         Add #include "mlan.h" to define TRUE,FALSE

#include "ds2480.h"
#include "mlan.h"

// external COM functions required
extern void FlushCOM(void);
extern int  WriteCOM(int, uchar *);
extern int  ReadCOM(int, uchar *);
//extern void SetBaudCOM(int);

// external DS2480 utility function
extern int DS2480Detect(void);
//extern int DS2480ChangeBaud(int);

// local functions
int  MLanTouchReset(void);
int  MLanTouchBit(int sendbit);
int  MLanTouchByte(int sendbyte);
int  MLanWriteByte(int sendbyte);
int  MLanReadByte(void);
int  MLanSpeed(int);
int  MLanLevel(int);
int  MLanProgramPulse(void);

// external globals
extern int UMode;   // current DS2480 command or data mode state
extern int UBaud;   // current DS2480 baud rate
extern int USpeed;  // current DS2480 MicroLAN communication speed
extern int ULevel;  // current DS2480 MicroLAN level

// local varable flag, true if program voltage available
static int ProgramAvailable=FALSE;


//--------------------------------------------------------------------------
// Reset all of the devices on the MicroLAN and return the result.
//
// Returns: TRUE(1):  presense pulse(s) detected, device(s) reset
//          FALSE(0): no presense pulses detected
//
// WARNING: This routine will not function correctly on some
//          Alarm reset types of the DS1994/DS1427/DS2404 with
//          Rev 1 and 2 of the DS2480.
//
int MLanTouchReset(void)
{
   uchar readbuffer[10],sendpacket[10];
   int sendlen=0;

   // make sure normal level
   MLanLevel(MODE_NORMAL);

   // check if correct mode 
   if (UMode != MODSEL_COMMAND)
   {
      UMode = MODSEL_COMMAND;
      sendpacket[sendlen++] = MODE_COMMAND;
   }

   // construct the command
   sendpacket[sendlen++] = (uchar)(CMD_COMM | FUNCTSEL_RESET | USpeed);

   // flush the buffers
   FlushCOM();

   // send the packet 
   if (WriteCOM(sendlen,sendpacket)) 
   {
      // read back the 1 byte response 
      if (ReadCOM(1,readbuffer) == 1)
      {
         // make sure this byte looks like a reset byte
         if (((readbuffer[0] & RB_RESET_MASK) == RB_PRESENCE) ||
             ((readbuffer[0] & RB_RESET_MASK) == RB_ALARMPRESENCE)) 
         {
            // check if programming voltage available
            ProgramAvailable = ((readbuffer[0] & 0x20) == 0x20); 
            return TRUE;
         }
         else
            return FALSE;
      }
   }

   // an error occured so re-sync with DS2480
   DS2480Detect();

   return FALSE;
}


//--------------------------------------------------------------------------
// Send 1 bit of communication to the MicroLAN and return the
// result 1 bit read from the MicroLAN.  The parameter 'sendbit'
// least significant bit is used and the least significant bit
// of the result is the return bit.
//
// 'sendbit' - the least significant bit is the bit to send
//
// Returns: 0:   0 bit read from sendbit
//          1:   1 bit read from sendbit
//
int MLanTouchBit(int sendbit)
{
   uchar readbuffer[10],sendpacket[10];
   int sendlen=0;

   // make sure normal level
   MLanLevel(MODE_NORMAL);

   // check if correct mode 
   if (UMode != MODSEL_COMMAND)
   {
      UMode = MODSEL_COMMAND;
      sendpacket[sendlen++] = MODE_COMMAND;
   }

   // construct the command
   sendpacket[sendlen] = (sendbit != 0) ? BITPOL_ONE : BITPOL_ZERO;
   sendpacket[sendlen++] |= CMD_COMM | FUNCTSEL_BIT | USpeed;

   // flush the buffers
   FlushCOM();

   // send the packet 
   if (WriteCOM(sendlen,sendpacket)) 
   {
      // read back the response 
      if (ReadCOM(1,readbuffer) == 1)
      {
         // interpret the response 
         if (((readbuffer[0] & 0xE0) == 0x80) &&
             ((readbuffer[0] & RB_BIT_MASK) == RB_BIT_ONE))
            return 1;
         else
            return 0;
      }
   }

   // an error occured so re-sync with DS2480
   DS2480Detect();

   return 0;
}


//--------------------------------------------------------------------------
// Send 8 bits of communication to the MicroLAN and verify that the
// 8 bits read from the MicroLAN is the same (write operation).  
// The parameter 'sendbyte' least significant 8 bits are used.
//
// 'sendbyte' - 8 bits to send (least significant byte)
//
// Returns:  TRUE: bytes written and echo was the same
//           FALSE: echo was not the same 
//
int MLanWriteByte(int sendbyte)
{
   return (MLanTouchByte(sendbyte) == sendbyte) ? TRUE : FALSE;
}


//--------------------------------------------------------------------------
// Send 8 bits of read communication to the MicroLAN and and return the
// result 8 bits read from the MicroLAN.   
//
// Returns:  8 bytes read from MicroLAN
//
int MLanReadByte(void)
{
   return MLanTouchByte(0xFF);
}


//--------------------------------------------------------------------------
// Send 8 bits of communication to the MicroLAN and return the
// result 8 bits read from the MicroLAN.  The parameter 'sendbyte'
// least significant 8 bits are used and the least significant 8 bits
// of the result is the return byte.
//
// 'sendbyte' - 8 bits to send (least significant byte)
//
// Returns:  8 bytes read from sendbyte
//
int MLanTouchByte(int sendbyte)
{
   uchar readbuffer[10],sendpacket[10];
   int sendlen=0;

   // make sure normal level
   MLanLevel(MODE_NORMAL);

   // check if correct mode 
   if (UMode != MODSEL_DATA)
   {
      UMode = MODSEL_DATA;
      sendpacket[sendlen++] = MODE_DATA;
   }

   // add the byte to send
   sendpacket[sendlen++] = (uchar)sendbyte;

   // check for duplication of data that looks like COMMAND mode 
   if (sendbyte == MODE_COMMAND) 
      sendpacket[sendlen++] = (uchar)sendbyte;

   // flush the buffers
   FlushCOM();

   // send the packet 
   if (WriteCOM(sendlen,sendpacket)) 
   {
      // read back the 1 byte response 
      if (ReadCOM(1,readbuffer) == 1)
      {
          // return the response 
          return (int)readbuffer[0];
      }
   }

   // an error occured so re-sync with DS2480
   DS2480Detect();

   return 0;
}


//--------------------------------------------------------------------------
// Set the MicroLAN communucation speed.  
//
// 'NewSpeed' - new speed defined as
//                MODE_NORMAL     0x00
//                MODE_OVERDRIVE  0x01
//
// Returns:  current MicroLAN speed 
//
int MLanSpeed(int NewSpeed)
{
   uchar sendpacket[5];
   short sendlen=0;
   int rt = FALSE;
   
   // check if change from current mode
   if (((NewSpeed == MODE_OVERDRIVE) &&
        (USpeed != SPEEDSEL_OD)) ||
       ((NewSpeed == MODE_NORMAL) &&
        (USpeed != SPEEDSEL_FLEX)))
   {
      if (NewSpeed == MODE_OVERDRIVE) 
      {
         // if overdrive then switch to 115200 baud
         if (DS2480ChangeBaud(PARMSET_57600) == PARMSET_57600)
         {
            USpeed = SPEEDSEL_OD;
            rt = TRUE;
         }
      }
      else if (NewSpeed == MODE_NORMAL) 
      {
         // else normal so set to 9600 baud
         if (DS2480ChangeBaud(PARMSET_9600) == PARMSET_9600)
         {
            USpeed = SPEEDSEL_FLEX;
            rt = TRUE;
         }
      }

      // if baud rate is set correctly then change DS2480 speed
      if (rt)
      {
         // check if correct mode 
         if (UMode != MODSEL_COMMAND)
         {
            UMode = MODSEL_COMMAND;
            sendpacket[sendlen++] = MODE_COMMAND;
         }

         // proceed to set the DS2480 communication speed
         sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_SEARCHOFF | USpeed;

         // send the packet 
         if (!WriteCOM(sendlen,sendpacket)) 
         {
            rt = FALSE;
            // lost communication with DS2480 then reset 
            DS2480Detect();
         }
      }
   }

   // return the current speed
   return (USpeed == SPEEDSEL_OD) ? MODE_OVERDRIVE : MODE_NORMAL;
}


//--------------------------------------------------------------------------
// Set the MicroLAN line level.  The values for NewLevel are
// as follows:
//
// 'NewLevel' - new level defined as
//                MODE_NORMAL     0x00
//                MODE_STRONG5    0x02
//                MODE_PROGRAM    0x04
//                MODE_BREAK      0x08 (not supported)
//
// Returns:  current MicroLAN level  
//
int MLanLevel(int NewLevel)
{
   uchar sendpacket[10],readbuffer[10];
   short sendlen=0;
   short rt=FALSE;

   // check if need to change level
   if (NewLevel != ULevel)
   {
      // check if just putting back to normal
      if (NewLevel == MODE_NORMAL)
      {
         // check if correct mode 
         if (UMode != MODSEL_COMMAND)
         {
            UMode = MODSEL_COMMAND;
            sendpacket[sendlen++] = MODE_COMMAND;
         }

         // stop pulse command
         sendpacket[sendlen++] = MODE_STOP_PULSE;
   
         // flush the buffers
         FlushCOM();

         // send the packet 
         if (WriteCOM(sendlen,sendpacket)) 
         {
            // read back the 1 byte response 
            if (ReadCOM(1,readbuffer) == 1)
            {
               // check response byte
               if ((readbuffer[0] & 0xE0) == 0xE0)
               {
                  rt = TRUE;
                  ULevel = MODE_NORMAL;
               }
            }
         }
      }
      // set new level
      else
      {
         // check if correct mode 
         if (UMode != MODSEL_COMMAND)
         {
            UMode = MODSEL_COMMAND;
            sendpacket[sendlen++] = MODE_COMMAND;
         }

         // strong 5 volts
         if (NewLevel == MODE_STRONG5)
         {
            // set the SPUD time value 
            sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_5VPULSE | PARMSET_infinite;
            // add the command to begin the pulse
            sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE | BITPOL_5V;
         }
         // 12 volts
         else if (NewLevel == MODE_PROGRAM)
         {
            // check if programming voltage available
            if (!ProgramAvailable)
               return MODE_NORMAL;

            // set the PPD time value 
            sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_12VPULSE | PARMSET_infinite;
            // add the command to begin the pulse
            sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE | BITPOL_12V;
         }

         // flush the buffers
         FlushCOM();

         // send the packet 
         if (WriteCOM(sendlen,sendpacket)) 
         {
            // read back the 1 byte response from setting time limit
            if (ReadCOM(1,readbuffer) == 1)
            {
               // check response byte
               if ((readbuffer[0] & 0x81) == 0)
               {
                  ULevel = NewLevel;
                  rt = TRUE;
               }
            }
         }
      }

      // if lost communication with DS2480 then reset 
      if (rt != TRUE)
         DS2480Detect();
   }

   // return the current level
   return ULevel;      
}


//--------------------------------------------------------------------------
// This procedure creates a fixed 480 microseconds 12 volt pulse 
// on the MicroLAN for programming EPROM iButtons.
//
// Returns:  TRUE  successful
//           FALSE program voltage not available  
//
int MLanProgramPulse(void)
{
   uchar sendpacket[10],readbuffer[10];
   short sendlen=0;

   // check if programming voltage available
   if (!ProgramAvailable)
      return FALSE;

   // make sure normal level
   MLanLevel(MODE_NORMAL);

   // check if correct mode 
   if (UMode != MODSEL_COMMAND)
   {
      UMode = MODSEL_COMMAND;
      sendpacket[sendlen++] = MODE_COMMAND;
   }

   // set the SPUD time value 
   sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_12VPULSE | PARMSET_512us;

   // pulse command
   sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_CHMOD | BITPOL_12V | SPEEDSEL_PULSE;
   
   // flush the buffers
   FlushCOM();

   // send the packet 
   if (WriteCOM(sendlen,sendpacket)) 
   {
      // read back the 2 byte response 
      if (ReadCOM(2,readbuffer) == 2)
      {
         // check response byte
         if (((readbuffer[0] | CMD_CONFIG) == 
                (CMD_CONFIG | PARMSEL_12VPULSE | PARMSET_512us)) &&
             ((readbuffer[1] & 0xFC) == 
                (0xFC & (CMD_COMM | FUNCTSEL_CHMOD | BITPOL_12V | SPEEDSEL_PULSE))))
            return TRUE;
      }
   }

   // an error occured so re-sync with DS2480
   DS2480Detect();

   return FALSE;
}

