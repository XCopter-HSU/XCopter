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
 *  ext. low (-150%) 0x1c20       900탎
 *  low (-100%)      0x2260       1100탎
 *  neutral (0%)     0x2ee0       1500탎
 *  high (100%)      0x3b60       1900탎
 *  ext. high(150%)  0x41a0       2100탎

 * Channel Mapping (not sure)
 *  1 Pitch
 *  2 Aileron
 *  3 Elevator
 *  4 Yaw
 *  5 Aux/Gyro on MX-12
 *  6 ESC
 *  7 Aux/Gyr
 */

#include <stdio.h>
#include <stdint.h>
#include <system.h>
#include <string.h>
#include <altera_avalon_uart_regs.h> //should not be required, since we only read from the RX register
#include "ParseSUMDRawFrame.h"
#include "SUMD.h"
#include "b_pwmdriver.h"

//volatile static uint16_t* rs232uart = (uint16_t) UART_0_BASE; //UART Base address




/**
 * Initiating UART Core with:
 * 115200 Baudrate
 * refert to /documentation/n2cpu_nii51010.pdf for more info
 */
void initUart()
{
	int control = ALTERA_AVALON_UART_CONTROL_TRDY_MSK |
	ALTERA_AVALON_UART_CONTROL_RRDY_MSK |
	ALTERA_AVALON_UART_CONTROL_E_MSK;
	IOWR_ALTERA_AVALON_UART_CONTROL(UART_0_BASE, control);

	int divisor = (int)(50000000/115200 +0.5);  //div will be 434
	IOWR_ALTERA_AVALON_UART_DIVISOR(UART_0_BASE, divisor); //setting Baudrate-divisor: div = clk / desired Baudrate + 0,5
}

int main()
{
	printf("Starting to read from UART");
	initUart();
	PWMDriver_init();

	int byteNum = 0;


	uchar_t* rawBytes = 0; //will point to a raw non parsed SUMD-Frame

	uchar_t rx = 0;

	while (1) //reading uart
	{
		if (ALTERA_AVALON_UART_CONTROL_RRDY_MSK & IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE))
		{
			rx = IORD_ALTERA_AVALON_UART_RXDATA(UART_0_BASE);
		}
		else
		{
			continue;
		}

		if (rx != SUMD_GRAUPNER_VENDOR_ID && byteNum == 0) //sync package, and dont inerrupt the filling of the rawByte Array
		{
			continue; //skip all following values unless it is the beginning of a package
		}

		if (byteNum < SUMD_FRAME_LENGTH && rx )
		{
			rawBytes[byteNum] = rx; //save raw Byte input
			byteNum++;
		}


		if (byteNum == SUMD_FRAME_LENGTH) //raw Bytes are now collected.
		{
			byteNum = 0;

			if(crcRawFrameData(rawBytes))  //
			{
				struct SUMD_Frame* parsedFrame = parseSUMDFrame(rawBytes);

				//printSUMDFrame(parsedFrame);
				uint8_t channel2 = 0;
				channel2 = (parsedFrame->channel_data[2] - 8800) * 0.03984375; //15200 - 8800 = 6400
				//printf("normalized Channel 2: %d \n", channel2);
				PWMDriver_setSignalWidth(channel2, PWM_1);
				destroySUMDFrame(parsedFrame); //free the in parseSUMDFrame() allocated Memory. Required to prevent a Memoryleak
			}else
			{
				printf("CRC ERROR");
			}

		}
	}

	return 0;
}
