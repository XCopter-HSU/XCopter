#ifndef _SUMD_H_
#define _SUMD_H_

typedef unsigned char uchar_t;
/*
 * this file contains Defenitions and Datatypes needed when working with SUMD
 */

/* SUMD frame size and contents definitions */
#define SUMD_HEADER_LENGTH 3
#define SUMD_CRC_LENGTH 2
#define SUMD_GRAUPNER_GR_16_NUM_CHANNELS 8
//this can vary depending on the used RC (in this case it's a GR-16)
#define SUMD_FRAME_LENGTH (SUMD_HEADER_LENGTH+SUMD_GRAUPNER_GR_16_NUM_CHANNELS*2+SUMD_CRC_LENGTH)
#define SUMD_GRAUPNER_VENDOR_ID 0xA8
#define SUMD_STATUS_LIVING_SUMD 0x01
#define SUMD_STATUS_FAILSAFE 0x81

#define SUMD_VENDOR_ID_BYTE_NUM 0
#define SUMD_STATUS_BYTE_NUM 1
#define SUMD_NUM_CHANNELS_BYTE_NUM 2
#define SUMD_CHANNEL_0_HIGH_BYTE_NUM 3
#define SUMD_CHANNEL_0_LOW_BYTE_NUM 4
#define SUMD_CHANNEL_1_HIGH_BYTE_NUM 5
#define SUMD_CHANNEL_1_LOW_BYTE_NUM 6
#define SUMD_CHANNEL_2_HIGH_BYTE_NUM 7
#define SUMD_CHANNEL_2_LOW_BYTE_NUM 8
#define SUMD_CHANNEL_3_HIGH_BYTE_NUM 9
#define SUMD_CHANNEL_3_LOW_BYTE_NUM 10
#define SUMD_CHANNEL_4_HIGH_BYTE_NUM 11
#define SUMD_CHANNEL_4_LOW_BYTE_NUM 12
#define SUMD_CHANNEL_5_HIGH_BYTE_NUM 13
#define SUMD_CHANNEL_5_LOW_BYTE_NUM 14
#define SUMD_CHANNEL_6_HIGH_BYTE_NUM 15
#define SUMD_CHANNEL_6_LOW_BYTE_NUM 16
#define SUMD_CHANNEL_7_HIGH_BYTE_NUM 17
#define SUMD_CHANNEL_7_LOW_BYTE_NUM 18
#define SUMD_CHANNEL_8_HIGH_BYTE_NUM 19
#define SUMD_CHANNEL_8_LOW_BYTE_NUM 20
#define SUMD_CRC_HIGH_BYTE_NUM 21
#define SUMD_CRC_LOW_BYTE_NUM 22

/*
 * SUMD Serial Port
 */
#define SUMD_SERIAL_BAUDRATE 115200
#define SUMD_SERIAL_BITS_NUM 8
#define SUMD_SERIAL_STOPBITS_NUM 1

struct SUMD_Frame {
	uint8_t vendorID;
	uint8_t status;
	uint16_t numChannels;
	//channel_data[0] is MSB of channel 0, channel_data[1] is LSB of channel 0
	uint16_t channel_data[SUMD_GRAUPNER_GR_16_NUM_CHANNELS * 2];
	uint16_t crcHighByte;
	uint16_t crcLowByte;
};

#endif
