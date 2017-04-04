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
//  MLanNetU.C - Network functions for MicroLAN 1-Wire devices
//               using the DS2480 (U) serial interface chip. 
//
//  Version: 1.03
//
//           1.02 -> 1.03  Removed caps in #includes for Linux capatibility
//

#include "ds2480.h"
#include "mlan.h"

// external MicroLAN functions required
extern int MLanTouchReset(void);
extern int MLanTouchBit(int);
extern int MLanWriteByte(int sendbyte);
extern int MLanReadByte(void);
extern int MLanSpeed(int);
extern int MLanLevel(int);
extern int MLanBlock(int, uchar *, int);

// external COM functions required
extern void FlushCOM(void);
extern int  WriteCOM(int, uchar *);
extern int  ReadCOM(int, uchar *);

// external DS2480 utility function
extern int DS2480Detect(void);

// exportable functions
int  MLanFirst(int,int);
int  MLanNext(int,int);
void MLanSerialNum(uchar *, int);
void MLanFamilySearchSetup(int);
void MLanSkipFamily(void);
int  MLanAccess(void);
int  MLanVerify(int);
int  MLanOverdriveAccess(void);

// local functions       
int bitacc(int, int, int, uchar *);
uchar dowcrc(uchar);

// global variables for this module to hold search state information
static int LastDiscrepancy;
static int LastFamilyDiscrepancy;
static int LastDevice;
uchar DOWCRC;
uchar SerialNum[8];

// external globals
extern int UMode;
extern int UBaud;
extern int USpeed;

//--------------------------------------------------------------------------
// The 'MLanFirst' finds the first device on the MicroLAN  This function 
// contains one parameter 'OnlyAlarmingDevices'.  When 
// 'OnlyAlarmingDevices' is TRUE (1) the find alarm command 0xEC is 
// sent instead of the normal search command 0xF0.
// Using the find alarm command 0xEC will limit the search to only
// 1-Wire devices that are in an 'alarm' state. 
//
// 'DoReset' - TRUE (1) perform reset before search, FALSE (0) do not
//             perform reset before search. 
// 'OnlyAlarmDevices' - TRUE (1) the find alarm command 0xEC is 
//             sent instead of the normal search command 0xF0
//
// Returns:   TRUE (1) : when a 1-Wire device was found and it's 
//                        Serial Number placed in the global SerialNum
//            FALSE (0): There are no devices on the MicroLAN.
// 
int MLanFirst(int DoReset, int OnlyAlarmingDevices)
{
   // reset the search state
   LastDiscrepancy = 0;
   LastDevice = FALSE;
   LastFamilyDiscrepancy = 0; 

   return MLanNext(DoReset, OnlyAlarmingDevices);
}

//--------------------------------------------------------------------------
// The 'MLanNext' function does a general search.  This function
// continues from the previos search state. The search state
// can be reset by using the 'MLanFirst' function.
// This function contains one parameter 'OnlyAlarmingDevices'.  
// When 'OnlyAlarmingDevices' is TRUE (1) the find alarm command 
// 0xEC is sent instead of the normal search command 0xF0.
// Using the find alarm command 0xEC will limit the search to only
// 1-Wire devices that are in an 'alarm' state. 
//
// 'DoReset' - TRUE (1) perform reset before search, FALSE (0) do not
//             perform reset before search. 
// 'OnlyAlarmDevices' - TRUE (1) the find alarm command 0xEC is 
//             sent instead of the normal search command 0xF0
//
// Returns:   TRUE (1) : when a 1-Wire device was found and it's 
//                       Serial Number placed in the global SerialNum
//            FALSE (0): when no new device was found.  Either the
//                       last search was the last device or there
//                       are no devices on the MicroLAN.
// 
int MLanNext(int DoReset, int OnlyAlarmingDevices)
{
   int i,TempLastDescrepancy,pos;
   uchar TempSerialNum[8];
   uchar readbuffer[20],sendpacket[40];
   int sendlen=0;

   // if the last call was the last one 
   if (LastDevice)
   {
      // reset the search
      LastDiscrepancy = 0;
      LastDevice = FALSE;
      LastFamilyDiscrepancy = 0;          
      return FALSE;
   }

   // check if reset first is requested
   if (DoReset)
   {
      // reset the 1-wire 
      // if there are no parts on 1-wire, return FALSE
      if (!MLanTouchReset())
      {
         // reset the search
         LastDiscrepancy = 0;        
         LastFamilyDiscrepancy = 0; 
         return FALSE;
      }
   }

   // build the command stream
   // call a function that may add the change mode command to the buff
   // check if correct mode 
   if (UMode != MODSEL_DATA)
   {
      UMode = MODSEL_DATA;
      sendpacket[sendlen++] = MODE_DATA;
   }

   // search command
   if (OnlyAlarmingDevices)
      sendpacket[sendlen++] = 0xEC; // issue the alarming search command 
   else
      sendpacket[sendlen++] = 0xF0; // issue the search command 

   // change back to command mode
   UMode = MODSEL_COMMAND;
   sendpacket[sendlen++] = MODE_COMMAND;

   // search mode on
   sendpacket[sendlen++] = (uchar)(CMD_COMM | FUNCTSEL_SEARCHON | USpeed);

   // change back to data mode
   UMode = MODSEL_DATA;
   sendpacket[sendlen++] = MODE_DATA;

   // set the temp Last Descrep to none
   TempLastDescrepancy = 0xFF;  

   // add the 16 bytes of the search
   pos = sendlen;
   for (i = 0; i < 16; i++)
      sendpacket[sendlen++] = 0;

   // only modify bits if not the first search
   if (LastDiscrepancy != 0xFF)
   {
      // set the bits in the added buffer
      for (i = 0; i < 64; i++)
      {
         // before last discrepancy
         if (i < (LastDiscrepancy - 1)) 
               bitacc(WRITE_FUNCTION,
                   bitacc(READ_FUNCTION,0,i,&SerialNum[0]), 
                   (short)(i * 2 + 1), 
                   &sendpacket[pos]);
         // at last discrepancy
         else if (i == (LastDiscrepancy - 1)) 
                bitacc(WRITE_FUNCTION,1, 
                   (short)(i * 2 + 1), 
                   &sendpacket[pos]);
         // after last discrepancy so leave zeros
      }
   }

   // change back to command mode
   UMode = MODSEL_COMMAND;
   sendpacket[sendlen++] = MODE_COMMAND;

   // search OFF
   sendpacket[sendlen++] = (uchar)(CMD_COMM | FUNCTSEL_SEARCHOFF | USpeed);

   // flush the buffers
   FlushCOM();

   // send the packet 
   if (WriteCOM(sendlen,sendpacket)) 
   {
      // read back the 1 byte response 
      if (ReadCOM(17,readbuffer) == 17)
      {
         // interpret the bit stream
         for (i = 0; i < 64; i++)
         {
            // get the SerialNum bit
            bitacc(WRITE_FUNCTION,
                   bitacc(READ_FUNCTION,0,(short)(i * 2 + 1),&readbuffer[1]),
                   i,
                   &TempSerialNum[0]);
            // check LastDiscrepancy
            if ((bitacc(READ_FUNCTION,0,(short)(i * 2),&readbuffer[1]) == 1) &&
                (bitacc(READ_FUNCTION,0,(short)(i * 2 + 1),&readbuffer[1]) == 0))
            {
               TempLastDescrepancy = i + 1;  
               // check LastFamilyDiscrepancy
               if (i < 8)
                  LastFamilyDiscrepancy = i + 1; 
            }
         }

         // do dowcrc
         DOWCRC = 0;
         for (i = 0; i < 8; i++)
            dowcrc(TempSerialNum[i]);

         // check results 
         if ((DOWCRC != 0) || (LastDiscrepancy == 63) || (TempSerialNum[0] == 0))
         {
            // error during search 
            // reset the search
            LastDiscrepancy = 0;
            LastDevice = FALSE;
            LastFamilyDiscrepancy = 0;          
            return FALSE;
         }
         // successful search
         else
         {
            // check for lastone
            if ((TempLastDescrepancy == LastDiscrepancy) || (TempLastDescrepancy == 0xFF))
               LastDevice = TRUE;

            // copy the SerialNum to the buffer
            for (i = 0; i < 8; i++)
               SerialNum[i] = TempSerialNum[i];
         
            // set the count
            LastDiscrepancy = TempLastDescrepancy;
            return TRUE;
         }
      }
   }

   // an error occured so re-sync with DS2480
   DS2480Detect();

   // reset the search
   LastDiscrepancy = 0;
   LastDevice = FALSE;
   LastFamilyDiscrepancy = 0;          

   return FALSE;
}


//--------------------------------------------------------------------------
// The 'MLanSerialNum' function either reads or sets the SerialNum buffer 
// that is used in the search functions 'MLanFirst' and 'MLanNext'.  
// This function contains two parameters, 'SerialNumBuf' is a pointer
// to a buffer provided by the caller.  'SerialNumBuf' should point to 
// an array of 8 unsigned chars.  The second parameter is a flag called
// 'DoRead' that is TRUE (1) if the operation is to read and FALSE
// (0) if the operation is to set the internal SerialNum buffer from 
// the data in the provided buffer.
//
// 'SerialNumBuf' - buffer to that contains the serial number to set
//                  when DoRead = FALSE (0) and buffer to get the serial
//                  number when DoRead = TRUE (1).
// 'DoRead'       - flag to indicate reading (1) or setting (0) the current
//                  serial number.
//
void MLanSerialNum(uchar *SerialNumBuf, int DoRead)
{
   int i;

   // read the internal buffer and place in 'SerialNumBuf'
   if (DoRead)
   {
      for (i = 0; i < 8; i++)
         SerialNumBuf[i] = SerialNum[i];
   }
   // set the internal buffer from the data in 'SerialNumBuf'
   else
   {
      for (i = 0; i < 8; i++)
         SerialNum[i] = SerialNumBuf[i];
   }
}


//--------------------------------------------------------------------------
// Setup the search algorithm to find a certain family of devices
// the next time a search function is called 'MLanNext'.
//
// 'SearchFamily' - family code type to set the search algorithm to find
//                  next.
// 
void MLanFamilySearchSetup(int SearchFamily)
{
   int i;

   // set the search state to find SearchFamily type devices
   SerialNum[0] = (uchar)SearchFamily;                 
   for (i = 1; i < 8; i++)
      SerialNum[i] = 0; 
   LastDiscrepancy = 64;     
   LastDevice = FALSE;          
}


//--------------------------------------------------------------------------
// Set the current search state to skip the current family code.
//
void MLanSkipFamily(void)
{
   // set the Last discrepancy to last family discrepancy
   LastDiscrepancy = LastFamilyDiscrepancy;

   // check for end of list
   if (LastDiscrepancy == 0) 
      LastDevice = TRUE;
}


//--------------------------------------------------------------------------
// The 'MLanAccess' function resets the 1-Wire and sends a MATCH Serial 
// Number command followed by the current SerialNum code. After this 
// function is complete the 1-Wire device is ready to accept device-specific
// commands. 
//
// Returns:   TRUE (1) : reset indicates present and device is ready
//                       for commands.
//            FALSE (0): reset does not indicate presence or echos 'writes'
//                       are not correct.
//
int MLanAccess(void)
{
   uchar TranBuf[9];
   int i;

   // reset the 1-wire 
   if (MLanTouchReset())
   {
      // create a buffer to use with block function      
      // match Serial Number command 0x55 
      TranBuf[0] = 0x55; 
      // Serial Number
      for (i = 1; i < 9; i++)
         TranBuf[i] = SerialNum[i-1];
      
      // send/recieve the transfer buffer   
      if (MLanBlock(FALSE,TranBuf,9))
      {
         // verify that the echo of the writes was correct
         for (i = 1; i < 9; i++)
            if (TranBuf[i] != SerialNum[i-1])
               return FALSE;
         if (TranBuf[0] != 0x55)
            return FALSE;
         else
            return TRUE;
      }
   }

   // reset or match echo failed
   return FALSE;
}


//----------------------------------------------------------------------
// The function 'MLanVerify' verifies that the current device
// is in contact with the MicroLAN.    
// Using the find alarm command 0xEC will verify that the device
// is in contact with the MicroLAN and is in an 'alarm' state. 
// 
// 'OnlyAlarmingDevices' - TRUE (1) the find alarm command 0xEC 
//                         is sent instead of the normal search 
//                         command 0xF0. 
//
// Returns:   TRUE (1) : when the 1-Wire device was verified
//                       to be on the MicroLAN 
//                       with OnlyAlarmingDevices == FALSE 
//                       or verified to be on the MicroLAN
//                       AND in an alarm state when 
//                       OnlyAlarmingDevices == TRUE. 
//            FALSE (0): the 1-Wire device was not on the 
//                       MicroLAN or if OnlyAlarmingDevices
//                       == TRUE, the device may be on the 
//                       MicroLAN but in a non-alarm state.
// 
int MLanVerify(int OnlyAlarmingDevices)
{
   int i,TranCnt=0,goodbits=0,cnt=0,s,tst;
   uchar TranBuf[50];
   
   // construct the search rom 
   if (OnlyAlarmingDevices)
      TranBuf[TranCnt++] = 0xEC; // issue the alarming search command 
   else
      TranBuf[TranCnt++] = 0xF0; // issue the search command 
   // set all bits at first
   for (i = 1; i <= 24; i++)
      TranBuf[TranCnt++] = 0xFF;   
   // now set or clear apropriate bits for search 
   for (i = 0; i < 64; i++)
      bitacc(WRITE_FUNCTION,bitacc(READ_FUNCTION,0,i,&SerialNum[0]),(int)((i+1)*3-1),&TranBuf[1]);

   // send/recieve the transfer buffer   
   if (MLanBlock(TRUE,TranBuf,TranCnt))
   {
      // check results to see if it was a success 
      for (i = 0; i < 192; i += 3)
      {
         tst = (bitacc(READ_FUNCTION,0,i,&TranBuf[1]) << 1) |
                bitacc(READ_FUNCTION,0,(int)(i+1),&TranBuf[1]);

         s = bitacc(READ_FUNCTION,0,cnt++,&SerialNum[0]);

         if (tst == 0x03)  // no device on line 
         {
              goodbits = 0;    // number of good bits set to zero 
              break;     // quit 
         }

         if (((s == 0x01) && (tst == 0x02)) ||
             ((s == 0x00) && (tst == 0x01))    )  // correct bit 
            goodbits++;  // count as a good bit 
      }

      // check too see if there were enough good bits to be successful 
      if (goodbits >= 8) 
         return TRUE;
   }

   // block fail or device not present
   return FALSE;
}


//----------------------------------------------------------------------
// Perform a overdrive MATCH command to select the 1-Wire device with 
// the address in the ID data register.
//
// Returns:  TRUE: If the device is present on the MicroLAN and
//                 can do overdrive then the device is selected.
//           FALSE: Device is not present or not capable of overdrive.
//
//  *Note: This function could be converted to send DS2480
//         commands in one packet.  
//
int MLanOverdriveAccess(void)
{
   uchar TranBuf[8];
   int i, EchoBad = FALSE;

   // make sure normal level
   MLanLevel(MODE_NORMAL);

   // force to normal communication speed
   MLanSpeed(MODE_NORMAL);

   // call the MicroLAN reset function 
   if (MLanTouchReset())
   {
      // send the match command 0x69
      if (MLanWriteByte(0x69))
      {
         // switch to overdrive communication speed
         MLanSpeed(MODE_OVERDRIVE);

         // create a buffer to use with block function      
         // Serial Number
         for (i = 0; i < 8; i++)
            TranBuf[i] = SerialNum[i];
      
         // send/recieve the transfer buffer   
         if (MLanBlock(FALSE,TranBuf,8))
         {
            // verify that the echo of the writes was correct
            for (i = 0; i < 8; i++)
               if (TranBuf[i] != SerialNum[i])
                  EchoBad = TRUE;
            // if echo ok then success
            if (!EchoBad)
               return TRUE;               
         }
      }
   }
   
   // failure, force back to normal communication speed
   MLanSpeed(MODE_NORMAL);

   return FALSE;
}


//--------------------------------------------------------------------------
// Update the Dallas Semiconductor One Wire CRC (DOWCRC) from the global
// variable DOWCRC and the argument.  
//
// 'x' - data byte to calculate the 8 bit crc from
//
// Returns: the updated DOWCRC.
//
uchar dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

uchar dowcrc(uchar x)
{
   DOWCRC = dscrc_table[DOWCRC ^ x];
   return DOWCRC;
}


//--------------------------------------------------------------------------
// Bit utility to read and write a bit in the buffer 'buf'.
//
// 'op'    - operation (1) to set and (0) to read
// 'state' - set (1) or clear (0) if operation is write (1)
// 'loc'   - bit number location to read or write
// 'buf'   - pointer to array of bytes that contains the bit
//           to read or write
//
// Returns: 1   if operation is set (1)
//          0/1 state of bit number 'loc' if operation is reading 
//
int bitacc(int op, int state, int loc, uchar *buf)
{
     int nbyt,nbit;

     nbyt = (loc / 8);
     nbit = loc - (nbyt * 8);

     if (op == WRITE_FUNCTION)
     {
          if (state)
             buf[nbyt] |= (0x01 << nbit);
          else
             buf[nbyt] &= ~(0x01 << nbit);

          return 1;
     }
     else
          return ((buf[nbyt] >> nbit) & 0x01);
}
