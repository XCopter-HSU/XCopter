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

#include "ParseSUMDRawFrame.h"

/*
 * checks crc 16 of received raw frame data
 * @parameters
 * 	received raw frame data as unsigned char pointer
 * @return
 * 	1 	-> CRC good
 * 	-1 	-> CRC wrong
 */
uint8_t crcRawFrameData(uchar_t* receivedRawFrameData) {
	/* check crc before processing */
	/* SUMD has 16 bit CCITT CRC */
	uint16_t crc = 0;
	uchar_t* bytePointer = &receivedRawFrameData[0];
	//crc only over header and data of frame
	int len = SUMD_FRAME_LENGTH - SUMD_CRC_LENGTH;
	int n=0, i=0;
	for (n = 0; n < len; n++) {
		crc ^= (uint16_t) bytePointer[n] << 8;
		for (i = 0; i < 8; i++)
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
	}
	if (crc ^ (((uint16_t) bytePointer[len] << 8) | bytePointer[len + 1]))
		/* wrong crc checksum found */
		return -1;
	else
		return 1;

}

/**
 * parse complete frame data
 * @parameters
 * 	raw byte data of a recieved sumd frame as unsigned char pointer
 * @return
 * 	pointer on filled sumd_frame struct if crc was good
 *	-1 if crc was wrong
 */
struct SUMD_Frame* parseSUMDFrame(uchar_t* receivedRawFrameData) {

	/* check the frame and crc for a valid HoTT SUMD data */
	if (crcRawFrameData(receivedRawFrameData)) {

		//allocate memory for sumdFrame
		struct SUMD_Frame* sumdFrame = malloc(sizeof(struct SUMD_Frame));

		if (sumdFrame != NULL) {
			//fill sumdFrame
			sumdFrame->vendorID = receivedRawFrameData[SUMD_VENDOR_ID_BYTE_NUM];
			sumdFrame->status = receivedRawFrameData[SUMD_STATUS_BYTE_NUM];
			sumdFrame->numChannels =
					receivedRawFrameData[SUMD_NUM_CHANNELS_BYTE_NUM];

			//pointer on channel bytes starting with channel 0 high byte
			uint8_t *tmp = &(receivedRawFrameData[SUMD_CHANNEL_0_HIGH_BYTE_NUM]);
			//loop over all channels (bytes)
			int i=0;
			for (i = 0; i < SUMD_GRAUPNER_GR_16_NUM_CHANNELS; i++) {
				//shift high byte by 8 and bitwise OR the low byte to create the 16 bit channel value
				sumdFrame->channel_data[i] = ((uint16_t) tmp[0] << 8) | tmp[1];
				//set channel pointer to next channel (increment by 16)
				tmp += 16;
			}
			return sumdFrame;
		}

	}
	return 0;

}

/*
 * destroys and frees the memory of a SUMD_Frame Pointer
 * @parameters
 * 	pointer on SUMD_Frame
 */
void destroySUMDFrame(struct SUMD_Frame* sumdFrame) {
	if (sumdFrame != NULL) {
		free(sumdFrame);
	}
}

/*
 * print a SUMD_Frame
 * @parameters
 * 	pointer on SUMD_Frame
 */
void printSUMDFrame(struct SUMD_Frame* sumdFrame) {
    printf("VendorID: %d\n", sumdFrame->vendorID);
    printf("\tStatus: %d\n", sumdFrame->status);
    printf("\tNumber of Channels in Frame: %d\n", sumdFrame->numChannels);

	int i=0;
    for(i=0;i<SUMD_GRAUPNER_GR_16_NUM_CHANNELS;i++)
        printf("\tChannel %i: %d\n", i, sumdFrame->channel_data[i]);

    printf("\tCRC16 High Byte: %x\n", sumdFrame->crcHighByte);
    printf("\tCRC16 Low Byte: %x\n", sumdFrame->crcLowByte);

}

