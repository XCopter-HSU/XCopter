#ifndef _PARSE_SUMD_RAW_DATA_FRAME_H_
#define _PARSE_SUMD_RAW_DATA_FRAME_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "SUMD.h"

/**
 * HSUM protocol documentation
 *
 * Currently Graupner HoTT serial port settings:
 *  115200bps serial stream, 8 bits, no parity, 1 stop bit
 *  size of each frame: 11..37 bytes
 *  data resolution: 14 bit
 *  frame period: 11ms or 22ms
 *
 * Currently known SUMD/SUMH frame structure:
 *  Section          Byte_Number        Byte_Name      Byte_Value Remark
 *  Header           0                  Vendor_ID      0xA8       Graupner
 *  Header			 1					Status                    0x01       valid and live SUMD data frame
 *                                                     0x81       valid SUMD data frame with
 *                                                                transmitter in fail safe condition
 *                                                     others     invalid frame
 *  Header           2                  N_Channels     0x02..0x20 number of transmitted channels
 *  Data             n*2+1              Channel n MSB  0x00..0xff High Byte of channel n data
 *  Data             n*2+2              Channel n LSB  0x00..0xff Low Byte of channel n data
 *  SUMD_CRC         (N_Channels+1)*2+1 CRC High Byte  0x00..0xff High Byte of 16 Bit CRC
 *  SUMD_CRC         (N_Channels+1)*2+2 CRC Low Byte   0x00..0xff Low Byte of 16 Bit CRC
 *
 * Channel Data Interpretation
 *  Stick Positon    Channel Data Remark
 *  ext. low (-150%) 0x1c20       900µs
 *  low (-100%)      0x2260       1100µs
 *  neutral (0%)     0x2ee0       1500µs
 *  high (100%)      0x3b60       1900µs
 *  ext. high(150%)  0x41a0       2100µs

 * Channel Mapping (not sure)
 *  1 Pitch
 *  2 Aileron
 *  3 Elevator
 *  4 Yaw
 *  5 Aux/Gyro on MX-12
 *  6 ESC
 *  7 Aux/Gyr
 */


/*
 * checks crc 16 of received raw frame data
 * @parameters
 * 	received raw frame data as unsigned char pointer
 * @return
 * 	1 	-> CRC good
 * 	-1 	-> CRC wrong
 */
uint8_t crcRawFrameData(uchar_t* receivedRawFrameData);

/**
 * parse complete frame data
 * @parameters
 * 	raw byte data of a recieved sumd frame as unsigned char pointer
 * @return
 * 	pointer on filled sumd_frame struct if crc was good
 *	-1 if crc was wrong
 */
struct SUMD_Frame* parseSUMDFrame(uchar_t* receivedRawFrameData);

/*
 * print a SUMD_Frame
 * @parameters
 * 	pointer on SUMD_Frame
 */
void printSUMDFrame(struct SUMD_Frame* sumdFrame);

/*
 * destroys and frees the memory of a SUMD_Frame Pointer
 * @parameters
 * 	pointer on SUMD_Frame
 */
void destroySUMDFrame(struct SUMD_Frame* sumdFrame);

#endif
