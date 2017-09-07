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
// MLan.H - Include file for MicroLAN library
//
// Version: 1.03
// History: 1.02 -> 1.03 Make sure uchar is not defined twice.
//

// Typedefs
#ifndef UCHAR
   #define UCHAR
   typedef unsigned char  uchar;
#endif
#if 0
// not needed most places - kenmcd
typedef unsigned short ushort;
typedef unsigned long  ulong;
#endif

// general defines 
#define WRITE_FUNCTION 1
#define READ_FUNCTION  0   

// error codes
#define READ_ERROR    -1
#define INVALID_DIR   -2       
#define NO_FILE       -3    
#define WRITE_ERROR   -4   
#define WRONG_TYPE    -5
#define FILE_TOO_BIG  -6

// Misc 
#define FALSE          0
#define TRUE           1

// mode bit flags
#define MODE_NORMAL                    0x00
#define MODE_OVERDRIVE                 0x01
#define MODE_STRONG5                   0x02
#define MODE_PROGRAM                   0x04
#define MODE_BREAK                     0x08

// product families
#define TEMP_FAMILY        0x10
#define SWITCH_FAMILY      0x12
#define COUNT_FAMILY       0x1D
#define DIR_FAMILY         0x01

// externs
extern uchar DOWCRC;

// debugging
extern int MLanDebug;

// function prototypes
extern int Aquire1WireNet(char *, char *, int);
extern void Release1WireNet(char *, int);
extern int MLanAccess(void);
extern int MLanBlock(int DoReset, uchar *TransferBuffer, int TransferLen);
extern void MLanFamilySearchSetup(int SearchFamily);
extern int MLanLevel(int NewLevel);
extern int MLanNext(int DoReset, int OnlyAlarmingDevices);
extern void MLanSerialNum(uchar *SerialNumBuf, int DoRead);
extern int MLanTouchByte(int sendbyte);
extern int MLanVerify(int OnlyAlarmingDevices);
extern uchar dowcrc(uchar x);
extern void msDelay(int len);
extern int DS2480ChangeBaud(uchar newbaud);
extern void SetBaudCOM(int new_baud);

