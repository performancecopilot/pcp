#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "mlan/mlan.h"

//----------------------------------------------------------------------
// Read the temperature of a DS1820
//
// 'SerialNum'   - Serial Number of DS1820 to read temperature from
// 'Temp '       - pointer to variable where that temperature will be 
//                 returned
//
// Returns: TRUE(1)  temperature has been read and verified
//          FALSE(0) could not read the temperature, perhaps device is not
//                   in contact
//
int ReadTemperature(uchar SerialNum[8], float *Temp)
{
   int rt=FALSE;
   uchar send_block[30];
   int send_cnt=0, tsht, i;
   float tmp,cr,cpc;
   DOWCRC = 0;

   // set the device serial number to the counter device
   MLanSerialNum(SerialNum,FALSE);

   // access the device 
   if (MLanAccess())
   {
      // send the convert temperature command
      MLanTouchByte(0x44);

      // set the MicroLAN to strong pull-up
      if (MLanLevel(MODE_STRONG5) != MODE_STRONG5)
         return FALSE;
 
      // sleep for 1 second
      msDelay(1000);

      // turn off the MicroLAN strong pull-up
      if (MLanLevel(MODE_NORMAL) != MODE_NORMAL)
         return FALSE;

      // access the device 
      if (MLanAccess())
      {
         // create a block to send that reads the temperature
         // read scratchpad command
         send_block[send_cnt++] = 0xBE;
         // now add the read bytes for data bytes and crc8
         for (i = 0; i < 9; i++)
            send_block[send_cnt++] = 0xFF;

         // now send the block
         if (MLanBlock(FALSE,send_block,send_cnt))
         {
            // perform the CRC8 on the last 8 bytes of packet
            for (i = send_cnt - 9; i < send_cnt; i++)
               dowcrc(send_block[i]);

            // verify CRC8 is correct
            if (DOWCRC == 0x00)
            {
               // calculate the high-res temperature
               tsht = send_block[1];
               if (send_block[2] & 0x01)
                  tsht |= -256;
               tmp = (float)(tsht/2);
               cr = send_block[7];
               cpc = send_block[8];
               if (cpc == 0)
                  return FALSE;   
               else
                  tmp = tmp - (float)0.25 + (cpc - cr)/cpc;
   
               *Temp = tmp;
               // success
               rt = TRUE;
            }
         }
      }
   }
   
   // return the result flag rt
   return rt;
}

/*
 * find next temperature sensor
 */
unsigned char *
nextsensor(void)
{
    static unsigned char	sn[8];
    static int			first = 1;

    if (first) {
	// set the search to first find for temperature family code
	MLanFamilySearchSetup(TEMP_FAMILY);
	first = 0;
    }

    for ( ; ; ) {
	// perform the search
	if (!MLanNext(TRUE, FALSE)) {
	    return NULL;
	}
	// get serial number and verify the family code
	MLanSerialNum(sn, TRUE);
	if (sn[0] == TEMP_FAMILY)
	    return sn;
    }
}
