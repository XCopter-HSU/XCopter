/*
 * This is the I2C Driver that was written for an modified I2C Controller from
 * opencores.org.
 */

//-----------------------Includes----------------------------------------------
#include "Driver_I2C.h"
#include "../b_errorcodes.h"

//-----------------------Defines-----------------------------------------------
#define I2CDriver_PRESCLOREG 	(*((volatile uint8_t *) 0x09000040)) // Here you have to put in the Base Address of the Controller
#define I2CDriver_PRESCHIREG 	(*((volatile uint8_t *) 0x09000044)) // Here you have to put in the Address of Prescale Register (HIGH)
#define I2CDriver_CONTROLREG	(*((volatile uint8_t *) 0x09000048)) // Here you have to put in the Address of Prescale Register (LOW)
#define I2CDriver_TRANSMITREG	(*((volatile uint8_t *) 0x0900004c)) // Here you have to put in the Address of Transmit Register
#define I2CDriver_RECEIVEREG	(*((volatile uint8_t *) 0x0900004c)) // Here you have to put in the Address of Receive Register
#define I2CDriver_COMMANDREG	(*((volatile uint8_t *) 0x09000050)) // Here you have to put in the Address of Command Register
#define I2CDriver_STATEREG		(*((volatile uint8_t *) 0x09000050)) // Here you have to put in the Address of State Register

//-----------------------Constants---------------------------------------------
static const uint8_t I2CENABLE_M = 0x80;    		// Enable I2C (Control Register Bit 7)
static const uint8_t START_M  =	0x80;  				// Set start flag (Command Register Bit 7)
static const uint8_t STOP_M = 0x40;     			// Set stop flag (Command Register Bit 6)
static const uint8_t WRITE_M = 0x10;        		// Set Write to Slave (Command Register Bit 4)
static const uint8_t READ_M = 0x20;        			// Set Read from Slave (Command Register Bit 5)
static const uint8_t TRANSFER_M = 0x02;        		// Transfer in progress
static const uint8_t CHECK_SLAVE_ACK_M = 0x80;      // 0 if there was an Ack from the Slave
static const uint8_t NO_ACK_FROM_MASTER_M =	0x08;   // Generate NACK when something has been received
static const uint8_t ACK_FROM_MASTER_M = 0x00;		// Generate ACK when something has been received

//-----------------------Attributes--------------------------------------------
enum DriverState I2CDriver_state = I2C_NOTAVAILABLE; //Represents the driver state

//-----------------------Method Implementation---------------------------------
/*
 * The init Function of the I2C Driver
 */
int8_t I2CDriver_init() {
	/* If I2C Driver is already Active oder Initialized
	 * don't init Driver again.
	 */
	if(I2CDriver_state != I2C_NOTAVAILABLE) {
		return ERR_I2C_WRONG_STATE;
	}
	/* Reset speeds in prescale Registers */
	I2CDriver_PRESCLOREG = 0xFF;
	I2CDriver_PRESCHIREG = 0xFF;
	/* Reset registers */
	I2CDriver_CONTROLREG = 0x00;
	I2CDriver_TRANSMITREG = 0x00;
	I2CDriver_COMMANDREG = 0x00;
	/* Set state to INITIALIZED */
	I2CDriver_state = I2C_INITIALIZED;
	return NO_ERR;
}

/*
 * Function open opens the I2C Controller for Reading and Writing.
 */
int8_t I2CDriver_open(enum ControllerSpeed speed) {
	if(I2CDriver_state != I2C_INITIALIZED) {
		return ERR_I2C_WRONG_STATE;
	}
	if(speed == I2C_100) {
		I2CDriver_PRESCLOREG = 0xff & 0x63;			// Set I2C Low Byte
		I2CDriver_PRESCHIREG = 0xff & (0x63>>8);	// Set I2C High Byte
	} else if(speed == I2C_400) {
		I2CDriver_PRESCLOREG = 0xff & 0x18;			// Set I2C Low Byte
		I2CDriver_PRESCHIREG = 0xff & (0x18>>8);	// Set I2C High Byte
	}
	I2CDriver_CONTROLREG = I2CENABLE_M;				// Enable I2C Functionality

	I2CDriver_state = I2C_ACTIVE;						// Change I2C Driver state

	return NO_ERR;
}

/*
 * Function close closes the I2C Controller
 */
int8_t I2CDriver_close() {
	/* Reset Prescale Registers */
	I2CDriver_PRESCLOREG = 0xFF;
	I2CDriver_PRESCHIREG = 0xFF;
	/* Disable I2C Functionality */
	I2CDriver_CONTROLREG = 0x00;
	/* Update I2CDriver State */
	I2CDriver_state = I2C_INITIALIZED;
	return NO_ERR;
}

/*
 * Function that writes 1 byte via the I2C Controller
 */
int8_t I2CDriver_write1Byte(uint8_t address, uint8_t data) {
	return I2CDriver_writeBytes(address,&data,1);	//Just calls more general write function
}

/*
 * Function that writes 2 bytes via the I2C Controller
 */
int8_t I2CDriver_write2Bytes(uint8_t address, uint8_t data0, uint8_t data1) {
	uint16_t data= data0 | (data1 << 8); 							//Construct 16bit value out of the two 8bit values
	return I2CDriver_writeBytes(address, (uint8_t*)(&data), 2); 	//Just calls more general write function
}

/*
 * Function that writes byte array via the I2C Controller.
 */
int8_t I2CDriver_writeByteArray(uint8_t address, uint8_t data[]) {
	return I2CDriver_writeBytes(address,data,sizeof(data)); 		//Just calls more general write function
}

/*
 * Function that writes a variable count of bytes via the I2C Controller.
 */
int8_t I2CDriver_writeBytes(uint8_t address, uint8_t *data, uint32_t count) {
	/* Check if I2C Driver state is ACTIVE, if not return error value */
	if(I2CDriver_state != I2C_ACTIVE) {
		return ERR_I2C_WRONG_STATE;
	}

	I2CDriver_TRANSMITREG = ((address << 1) | 0x00); 	// Write address of slave to Transmit register
	uint8_t currentCommand = START_M | WRITE_M;
	if(count == 0) {
		currentCommand = currentCommand | STOP_M; 		//If the count parameter is 0 add stop condition to command
	}
	I2CDriver_COMMANDREG = currentCommand; 				// Generate start condition and begin writing

	/* Wait until address has been transmitted */
	while(I2CDriver_STATEREG & TRANSFER_M);

	/* Check if slave generated an ACK, if not return error value */
	if(I2CDriver_STATEREG & CHECK_SLAVE_ACK_M) {
		I2CDriver_COMMANDREG = STOP_M;
		return ERR_NO_ACK_ADDR;
	}

	uint32_t sent; 										// Count variable for the bytes that have been already sent

	/* Loop until all Bytes have been sent */
	for(sent = 0; sent < count; sent++) {
		I2CDriver_TRANSMITREG = *(data+sent); 			// Set content of transmit register to current pointer location
		currentCommand = WRITE_M;
		/* If this will be the last byte command has
		 * also to contain stop condition
		 */
		if(sent == (count-1)) {
			currentCommand = currentCommand | STOP_M;
		}
		I2CDriver_COMMANDREG = currentCommand; 			// Set command register to write mask to start writing
		/* Wait until byte has been sent */
		while(I2CDriver_STATEREG & TRANSFER_M);

		/* Check if slave generated an ACK, if not return error value */
		if(I2CDriver_STATEREG & CHECK_SLAVE_ACK_M) {
			return ERR_NO_ACK_DATA;
		}
	}
	return NO_ERR; 											//Return error value 0 for success
}

/*
 * Function that reads 1 byte via the I2C Controller.
 */
int8_t I2CDriver_read1Byte(uint8_t address, uint8_t *data) {
	return I2CDriver_readBytes(address, data, 1);
}

/*
 * Function that reads 2 bytes via the I2C Controller.
 */
int8_t I2CDriver_read2Bytes(uint8_t address, uint8_t *data0, uint8_t *data1) {
	uint16_t data = 0; //Generate 16bit value to save value in
	/* Call more general read function */
	int errorValue = I2CDriver_readBytes(address, (uint8_t*)(&data), 2);
	/* Split bytes and return */
	*data0 = data;
	*data1 = (data>>8);
	return errorValue;
}

/*
 * Function that reads an array of bytes via the I2C Controller.
 */
int8_t I2CDriver_readByteArray(uint8_t address, uint8_t data[]) {
	return I2CDriver_readBytes(address, data, sizeof(data)); 	//Just calls more general read function
}

/*
 * Function that reads a variable length of bytes from the I2C Controller.
 */
int8_t I2CDriver_readBytes(uint8_t address, uint8_t *data, uint32_t count) {
	/* Check for state and return error code when I2C Driver has not been opened before */
	if(I2CDriver_state != I2C_ACTIVE) {
		return ERR_I2C_WRONG_STATE;
	}
	uint8_t currentCommand = START_M | WRITE_M;
	/*If the count is 0 the command has also to contain StopBit */
	if(count == 0) {
		currentCommand = currentCommand | NO_ACK_FROM_MASTER_M | STOP_M;
	}
	I2CDriver_TRANSMITREG = ((address << 1) | 0x01); 	// Write address with set read bit to transmit register
	I2CDriver_COMMANDREG = currentCommand; 				// Set start flag and enable writing to slave

	/* Wait until address has been transmitted */
	while(I2CDriver_STATEREG & TRANSFER_M);

	/* Check if Acknowledge of slave has been received */
	if(I2CDriver_STATEREG & CHECK_SLAVE_ACK_M) {
		I2CDriver_COMMANDREG = STOP_M;
		return ERR_NO_ACK_ADDR;
	}

	uint32_t received; 									// Variable to count already received bytes

	/* Loop until enough bytes have been received */
	for(received = 0; received < count; received++) {
		currentCommand = READ_M;
		/*If this is the last byte to read add StopBit to command */
		if(received == (count-1)) {
			currentCommand = currentCommand | NO_ACK_FROM_MASTER_M | STOP_M;
		}
		I2CDriver_COMMANDREG = currentCommand; 			// Set read mask to Command register
		/* Wait until byte received */
		while(I2CDriver_STATEREG & TRANSFER_M);
		*(data+received) = I2CDriver_RECEIVEREG; 		// Save Byte on pointer address
	}

	return NO_ERR; 										// Return Error Value 0 for success
}


int8_t I2CDriver_writeReadBytes(uint8_t address, uint8_t *dataWrite, uint8_t *dataRead, uint32_t countWrite, uint32_t countRead) {
	/* Check if I2C Driver state is ACTIVE, if not return error value */
	if(I2CDriver_state != I2C_ACTIVE) {
		return ERR_I2C_WRONG_STATE;
	}
	I2CDriver_TRANSMITREG = ((address << 1) | 0x00); 	// Write address of slave to Transmit register
	uint8_t currentCommand = START_M | WRITE_M;
	I2CDriver_COMMANDREG = currentCommand;				// Generate start condition and begin writing

	/* Wait until address has been transmitted */
	while(I2CDriver_STATEREG & TRANSFER_M);

	/* Check if slave generated an ACK, if not return error value */
	if(I2CDriver_STATEREG & CHECK_SLAVE_ACK_M) {
		return ERR_NO_ACK_ADDR;
	}

	uint32_t sent; 										// Count variable for the bytes that have been already sent

	/* Loop until all Bytes have been sent */
	for(sent = 0; sent < countWrite; sent++) {
		I2CDriver_TRANSMITREG = *(dataWrite+sent); 		// Set content of transmit register to current pointer location
		currentCommand = WRITE_M;
		I2CDriver_COMMANDREG = currentCommand; 			// Set command register to write mask to start writing

		/* Wait until byte has been sent */
		while(I2CDriver_STATEREG & TRANSFER_M);

		/* Check if slave generated an ACK, if not return error value */
		if(I2CDriver_STATEREG & CHECK_SLAVE_ACK_M) {
			return ERR_NO_ACK_DATA;
		}
	}

	/* Change transfer direction from here */
	I2CDriver_TRANSMITREG = ((address << 1) | 0x01); 	// Write address with set read bit to transmit register
	currentCommand = START_M | WRITE_M;
	I2CDriver_COMMANDREG = currentCommand; 				// Set start flag and enable writing to slave

	/* Wait until address has been transmitted */
	while(I2CDriver_STATEREG & TRANSFER_M);

	/* Check if Acknowledge of slave has been received */
	if(I2CDriver_STATEREG & CHECK_SLAVE_ACK_M) {
		return ERR_NO_ACK_ADDR;
	}

	uint32_t received; 									// Variable to count already received bytes

	/* Loop until enough bytes have been received */
	for(received = 0; received < countRead; received++) {
		/* If this is the last byte to receive command has to contain
		 * read bit, nack bit, stop bit
		 * If not command has to contain read bit, ack bit
		 */
		if(received == countRead - 1) {
			currentCommand = currentCommand | NO_ACK_FROM_MASTER_M | STOP_M;
		} else {
			currentCommand = READ_M | ACK_FROM_MASTER_M;
		}
		I2CDriver_COMMANDREG = currentCommand;
		/*Wait until byte received*/
		while(I2CDriver_STATEREG & TRANSFER_M);
		*(dataRead+received) = I2CDriver_RECEIVEREG; 	// Save Byte on pointer address
	}
	return NO_ERR;
}
