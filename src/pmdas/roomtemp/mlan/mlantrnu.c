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
//  MLanTranU.C - Transport functions for MicroLAN 1-Wire devices
//                using the DS2480 (U) serial interface chip.
//
//  Version: 1.03
//
//           1.02 -> 1.03  Removed caps in #includes for Linux capatibility
//

#include "ds2480.h"
#include "mlan.h"

// external low-level functions required
extern int MLanTouchReset(void);
extern int MLanWriteByte(int);
extern int MLanReadByte(void);
extern int MLanProgramPulse(void);

// external network-level functions required
extern int MLanAccess();

// external COM functions required
extern void FlushCOM(void);
extern int  WriteCOM(int, uchar *);
extern int  ReadCOM(int, uchar *);

// other external functions
extern int DS2480Detect(void);
extern uchar dowcrc(uchar);

// external globals
extern int UMode;
extern int UBaud;
extern int USpeed;
extern uchar SerialNum[8];
extern uchar DOWCRC;

// local exportable functions
int MLanBlock(int, uchar *, int);
int MLanReadPacketStd(int, int, uchar *);
int MLanWritePacketStd(int, uchar *, int, int, int);   
int MLanProgramByte(int, int, int, int, int);

// local functions       
static int Write_Scratchpad(uchar *, int, int);
static int Copy_Scratchpad(int, int);
unsigned short crc16(int);

// global variable
unsigned short CRC16;


//--------------------------------------------------------------------------
// The 'MLanBlock' transfers a block of data to and from the 
// MicroLAN with an optional reset at the begining of communication.
// The result is returned in the same buffer.
//
// 'DoReset' - cause a MLanTouchReset to occure at the begining of 
//             communication TRUE(1) or not FALSE(0)
// 'TransferBuffer' -  pointer to a block of unsigned
//             chars of length 'TranferLength' that will be sent 
//             to the MicroLAN
// 'TranferLength' - length in bytes to transfer

// Supported devices: all 
//
// Returns:   TRUE (1) : The optional reset returned a valid 
//                       presence (DoReset == TRUE) or there
//                       was no reset required.
//            FALSE (0): The reset did not return a valid prsence
//                       (DoReset == TRUE).
//
//  The maximum TransferLength is 64
//
int MLanBlock(int DoReset, uchar *TransferBuffer, int TransferLen)
{
   uchar sendpacket[150];
   int sendlen=0,i;

   // check for a block too big
   if (TransferLen > 64)
      return FALSE;

   // check if need to do a MLanTouchReset first
   if (DoReset)
   {
      if (!MLanTouchReset())
         return FALSE;
   }  

   // construct the packet to send to the DS2480
   // check if correct mode 
   if (UMode != MODSEL_DATA)
   {
      UMode = MODSEL_DATA;
      sendpacket[sendlen++] = MODE_DATA;
   }

   // add the bytes to send
   for (i = 0; i < TransferLen; i++)
   {
      sendpacket[sendlen++] = TransferBuffer[i];

      // check for duplication of data that looks like COMMAND mode 
      if (TransferBuffer[i] == MODE_COMMAND) 
         sendpacket[sendlen++] = TransferBuffer[i];
   }

   // flush the buffers
   FlushCOM();

   // send the packet 
   if (WriteCOM(sendlen,sendpacket)) 
   {
      // read back the response 
      if (ReadCOM(TransferLen,TransferBuffer) == TransferLen)
         return TRUE;
   }

   // an error occured so re-sync with DS2480
   DS2480Detect();

   return FALSE;
}
  

//--------------------------------------------------------------------------
// Read a Universal Data Packet from a standard NVRAM iButton 
// and return it in the provided buffer. The page that the 
// packet resides on is 'StartPage'.  Note that this function is limited 
// to single page packets. The buffer 'ReadBuffer' must be at least 
// 29 bytes long.  
//
// The Universal Data Packet always start on page boundaries but 
// can end anywhere.  The length is the number of data bytes not 
// including the length byte and the CRC16 bytes.  There is one 
// length byte. The CRC16 is first initialized to the starting 
// page number.  This provides a check to verify the page that 
// was intended is being read.  The CRC16 is then calculated over 
// the length and data bytes.  The CRC16 is then inverted and stored 
// low byte first followed by the high byte. 
//
// Supported devices: DS1992, DS1993, DS1994, DS1995, DS1996, DS1982, 
//                    DS1985, DS1986, DS2407, and DS1971. 
//
// 'DoAccess' - flag to indicate if an 'MLanAccess' should be
//              peformed at the begining of the read.  This may
//              be FALSE (0) if the previous call was to read the
//              previous page (StartPage-1).
// 'StartPage' - page number to start the read from 
// 'ReadBuffer' - pointer to a location to store the data read
//
// Returns:  >=0 success, number of data bytes in the buffer
//           -1  failed to read a valid UDP 
//     
//
int MLanReadPacketStd(int DoAccess, int StartPage, uchar *ReadBuffer)
{
   int i,length,TranCnt=0,HeadLen=0;
   uchar TranBuf[50];

   // check if access header is done 
   // (only use if in sequention read with one access at begining)
   if (DoAccess)
   {
      // match command
      TranBuf[TranCnt++] = 0x55;    
      for (i = 0; i < 8; i++)
         TranBuf[TranCnt++] = SerialNum[i];
      // read memory command
      TranBuf[TranCnt++] = 0xF0;     
      // write the target address
      TranBuf[TranCnt++] = ((StartPage << 5) & 0xFF);    
      TranBuf[TranCnt++] = (StartPage >> 3);
      // check for DS1982 exception (redirection byte)
      if (SerialNum[0] == 0x09)
         TranBuf[TranCnt++] = 0xFF;
      // record the header length
      HeadLen = TranCnt;
   }
   // read the entire page length byte
   for (i = 0; i < 32; i++)      
      TranBuf[TranCnt++] = 0xFF;   

   // send/recieve the transfer buffer   
   if (MLanBlock(DoAccess,TranBuf,TranCnt))
   {
      // seed crc with page number
      CRC16 = StartPage;               

      // attempt to read UDP from TranBuf
      length = TranBuf[HeadLen];            
      crc16(length);

      // verify length is not too large
      if (length <= 29)                
      {
         // loop to read packet including CRC
         for (i = 0; i < length; i++)     
         {
             ReadBuffer[i] = TranBuf[i+1+HeadLen];
             crc16(ReadBuffer[i]);           
         }
            
         // read and compute the CRC16 
         crc16(TranBuf[i+1+HeadLen]);
         crc16(TranBuf[i+2+HeadLen]);
         
         // verify the CRC16 is correct           
         if (CRC16 == 0xB001) 
           return length;        // return number of byte in record
      }  
   }

   // failed block or incorrect CRC
   return -1;
}


//--------------------------------------------------------------------------
// Write a Universal Data Packet onto a standard NVRAM 1-Wire device
// on page 'StartPage'.  This function is limited to UDPs that
// fit on one page.  The data to write is provided as a buffer
// 'WriteBuffer' with a length 'WriteLength'.
//
// The Universal Data Packet always start on page boundaries but 
// can end anywhere.  The length is the number of data bytes not 
// including the length byte and the CRC16 bytes.  There is one 
// length byte. The CRC16 is first initialized to the starting 
// page number.  This provides a check to verify the page that 
// was intended is being read.  The CRC16 is then calculated over 
// the length and data bytes.  The CRC16 is then inverted and stored 
// low byte first followed by the high byte. 
//
// Supported devices: DeviceEPROM=0 
//                        DS1992, DS1993, DS1994, DS1995, DS1996
//                    DeviceEPROM=1, EPROMCRCType=0(CRC8)
//                        DS1982
//                    DeviceEPROM=1, EPROMCRCType=1(CRC16) 
//                        DS1985, DS1986, DS2407 
//
// 'StartPage'    - page number to write packet to
// 'WriteBuffer'  - pointer to buffer containing data to write
// 'WriteLength'  - number of data byte in WriteBuffer
// 'DeviceEPROM'  - flag set if device is an EPROM (1 EPROM, 0 NVRAM)
// 'EPROMCRCType' - if DeviceEPROM=1 then indicates CRC type 
//                  (0 CRC8, 1 CRC16)
//
// Returns: TRUE(1)  success, packet written
//          FALSE(0) failure to write, contact lost or device locked
//
//
int MLanWritePacketStd(int StartPage, uchar *WriteBuffer, 
                       int WriteLength, int DeviceEPROM, int EPROMCRCType)
{
   uchar construct_buffer[32];
   int i,buffer_cnt=0,start_address,do_access;

   // check to see if data too long to fit on device
   if (WriteLength > 29)
     return FALSE;
              
   // seed crc with page number           
   CRC16 = StartPage; 
      
   // set length byte
   construct_buffer[buffer_cnt++] = (uchar)(WriteLength);
   crc16(WriteLength);
      
   // fill in the data to write
   for (i = 0; i < WriteLength; i++)
   {
     crc16(WriteBuffer[i]);
     construct_buffer[buffer_cnt++] = WriteBuffer[i];
   }  
      
   // add the crc
   construct_buffer[buffer_cnt++] = (uchar)(~(CRC16 & 0xFF));
   construct_buffer[buffer_cnt++] = (uchar)(~((CRC16 & 0xFF00) >> 8));
   
   // check if not EPROM                 
   if (!DeviceEPROM)
   {
      // write the page
      if (!Write_Scratchpad(construct_buffer,StartPage,buffer_cnt))
         return FALSE;
   
      // copy the scratchpad            
      if (!Copy_Scratchpad(StartPage,buffer_cnt))
         return FALSE;
     
      // copy scratch pad was good then success
      return TRUE;
   }
   // is EPROM
   else
   {  
      // calculate the start address
      start_address = ((StartPage >> 3) << 8) | ((StartPage << 5) & 0xFF);
      do_access = TRUE;
      // loop to program each byte
      for (i = 0; i < buffer_cnt; i++)
      {
         if (MLanProgramByte(construct_buffer[i], start_address + i, 
             0x0F, EPROMCRCType, do_access) != construct_buffer[i])
            return FALSE;
         do_access = FALSE;
      }
      return TRUE;
   }
}


//--------------------------------------------------------------------------
// Write a byte to an EPROM 1-Wire device.
//
// Supported devices: CRCType=0(CRC8)
//                        DS1982
//                    CRCType=1(CRC16) 
//                        DS1985, DS1986, DS2407 
//
// 'WRByte'       - byte to program
// 'Addr'         - address of byte to program
// 'WriteCommand' - command used to write (0x0F reg mem, 0x55 status)
// 'CRCType'      - CRC used (0 CRC8, 1 CRC16)
// 'DoAccess'     - Flag to access device for each byte 
//                  (0 skip access, 1 do the access)
//                  WARNING, only use DoAccess=0 if programing the NEXT
//                  byte immediatly after the previous byte.
//
// Returns: >=0   success, this is the resulting byte from the program
//                effort
//          -1    error, device not connected or program pulse voltage
//                not available
//
int MLanProgramByte(int WRByte, int Addr, int WriteCommand, 
                    int CRCType, int DoAccess)
{                                
   // optionally access the device
   if (DoAccess)
   {
      if (!MLanAccess())
         return -1;

      // send the write command
      if (!MLanWriteByte(WriteCommand))
         return -1;

      // send the address
      if (!MLanWriteByte(Addr & 0xFF))
         return -1;
      if (!MLanWriteByte(Addr >> 8))
         return -1;
   }

   // send the data to write
   if (!MLanWriteByte(WRByte))
      return -1;

   // read the CRC
   if (CRCType == 0)
   {
      // calculate CRC8
      if (DoAccess)
      {
         DOWCRC = 0;
         dowcrc((uchar)WriteCommand);
         dowcrc((uchar)(Addr & 0xFF));
         dowcrc((uchar)(Addr >> 8));
      }
      else
         DOWCRC = (uchar)(Addr & 0xFF);

      dowcrc((uchar)WRByte);
      // read and calculate the read crc
      dowcrc((uchar)MLanReadByte());
      // crc should now be 0x00
      if (DOWCRC != 0)
         return -1;
   }
   else
   {
      // CRC16
      if (DoAccess)
      {
         CRC16 = 0;
         crc16(WriteCommand);
         crc16(Addr & 0xFF);
         crc16(Addr >> 8);
      }
      else
         CRC16 = Addr;
      crc16(WRByte);
      // read and calculate the read crc
      crc16(MLanReadByte());
      crc16(MLanReadByte());
      // crc should now be 0xB001
      if (CRC16 != 0xB001)
         return -1;
   }

   // send the program pulse
   if (!MLanProgramPulse())
      return -1;

   // read back and return the resulting byte   
   return MLanReadByte();
}

                       
//--------------------------------------------------------------------------
// Write the scratchpad of a standard NVRam device such as the DS1992,3,4
// and verify its contents. 
//
// 'WriteBuffer'  - pointer to buffer containing data to write
// 'StartPage'    - page number to write packet to
// 'WriteLength'  - number of data byte in WriteBuffer
//
// Returns: TRUE(1)  success, the data was written and verified
//          FALSE(0) failure, the data could not be written
// 
//
int Write_Scratchpad(uchar *WriteBuffer, int StartPage, int WriteLength)
{
   int i,TranCnt=0;
   uchar TranBuf[50];
   
   // match command
   TranBuf[TranCnt++] = 0x55;    
   for (i = 0; i < 8; i++)
      TranBuf[TranCnt++] = SerialNum[i];
   // write scratchpad command
   TranBuf[TranCnt++] = 0x0F;     
   // write the target address
   TranBuf[TranCnt++] = ((StartPage << 5) & 0xFF);    
   TranBuf[TranCnt++] = (StartPage >> 3);

   // write packet bytes 
   for (i = 0; i < WriteLength; i++)
      TranBuf[TranCnt++] = WriteBuffer[i];
   
   // send/recieve the transfer buffer   
   if (MLanBlock(TRUE,TranBuf,TranCnt))
   {
      // now attempt to read back to check
      TranCnt = 0;
      // match command
      TranBuf[TranCnt++] = 0x55;    
      for (i = 0; i < 8; i++)
         TranBuf[TranCnt++] = SerialNum[i];
      // read scratchpad command
      TranBuf[TranCnt++] = 0xAA;     
      // read the target address, offset and data
      for (i = 0; i < (WriteLength + 3); i++)
         TranBuf[TranCnt++] = 0xFF;
   
      // send/recieve the transfer buffer   
      if (MLanBlock(TRUE,TranBuf,TranCnt))
      {
         // check address and offset of scratchpad read
         if ((TranBuf[10] != (int)((StartPage << 5) & 0xFF)) ||
             (TranBuf[11] != (int)(StartPage >> 3)) ||
             (TranBuf[12] != (int)(WriteLength - 1)))
            return FALSE;

         // verify each data byte
         for (i = 0; i < WriteLength; i++)
            if (TranBuf[i+13] != WriteBuffer[i])
               return FALSE;

         // must have verified
         return TRUE;
      }
   }
   
   // failed a block tranfer
   return FALSE;
}


//--------------------------------------------------------------------------
// Copy the contents of the scratchpad to its intended nv ram page.  The
// page and length of the data is needed to build the authorization bytes
// to copy.
//
// 'StartPage'    - page number to write packet to
// 'WriteLength'  - number of data bytes that are being copied
//
// Returns: TRUE(1)  success
//          FALSE(0) failure
//
int Copy_Scratchpad(int StartPage, int WriteLength)
{
   int i,TranCnt=0;
   uchar TranBuf[50];
   
   // match command
   TranBuf[TranCnt++] = 0x55;    
   for (i = 0; i < 8; i++)
      TranBuf[TranCnt++] = SerialNum[i];
   // copy scratchpad command
   TranBuf[TranCnt++] = 0x55;     
   // write the target address
   TranBuf[TranCnt++] = ((StartPage << 5) & 0xFF);    
   TranBuf[TranCnt++] = (StartPage >> 3);
   TranBuf[TranCnt++] = WriteLength - 1;
   // read copy result
   TranBuf[TranCnt++] = 0xFF;

   // send/recieve the transfer buffer   
   if (MLanBlock(TRUE,TranBuf,TranCnt))
   {
      // check address and offset of scratchpad read
      if ((TranBuf[10] != (int)((StartPage << 5) & 0xFF)) ||
          (TranBuf[11] != (int)(StartPage >> 3)) ||
          (TranBuf[12] != (int)(WriteLength - 1)) ||
          (TranBuf[13] & 0xF0))
         return FALSE;
      else
         return TRUE;   
   }
      
   // failed a block tranfer
   return FALSE;
}
                       
     
//--------------------------------------------------------------------------
// Calculate a new CRC16 from the input data integer.  Return the current
// CRC16 and also update the global variable CRC16.
//
// 'data' - data to perform a CRC16 on
//
// Returns: the current CRC16
//
static short oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

unsigned short crc16(int data)
{
   data = (data ^ (CRC16 & 0xff)) & 0xff;
   CRC16 >>= 8;

   if (oddparity[data & 0xf] ^ oddparity[data >> 4])
      CRC16 ^= 0xc001;

   data <<= 6;
   CRC16   ^= data;
   data <<= 1;
   CRC16   ^= data;

   return CRC16;
}
     
   
