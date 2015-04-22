/*************************************************************
 * file: util_packet_handling.c
 *
 * DESCRIPTION:
 * Provides functions to add data items of specific size to
 * a packet stored in memory, or to extract (get)
 * a data item out of a packet.
 *
 * EDITION HISTORY:
 * 2013-12-09: extracted from another file - ms (M. Strahnen)
 *************************************************************/
#include <stdint.h>

/**************************************************************
 * void pack_add_8  (uint8_t *buffer, int byteNr, int8_t value)
 * void pack_add_u8 (uint8_t *buffer, int byteNr, uint8_t value)
 * void pack_add_16 (uint8_t *buffer, int byteNr, int16_t value)
 * void pack_add_u16(uint8_t *buffer, int byteNr, uint16_t value)
 * void pack_add_32 (uint8_t *buffer, int byteNr, int32_t value)
 * void pack_add_u32(uint8_t *buffer, int byteNr, uint32_t value)
 * void pack_add_64 (uint8_t *buffer, int byteNr, int64_t value)
 * void pack_add_u64(uint8_t *buffer, int byteNr, uint64_t value)
 * void pack_add_flt(uint8_t *buffer, int byteNr, float value)
 * void pack_add_adr(uint8_t *buffer, int byteNr, uint32_t value)
 *
 * Functions will add parameter value to a buffer pointed to
 * by *(buffer + byteNr).
 *
 * Have Care!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Functions are for a CPU with little endian byte ordering,
 * like the NIOS II CPU. For a CPU with big endian byte
 * ordering you have to rewrite the functions.
 *************************************************************/
void pack_add_8(uint8_t *buffer, int byteNr, int8_t value) {
	*((int8_t*)(buffer + byteNr)) = value;
}

void pack_add_u8(uint8_t *buffer, int byteNr, uint8_t value) {
	*((uint8_t*)(buffer + byteNr)) = value;
}

void pack_add_16(uint8_t *buffer, int byteNr, int16_t value)  {
	*((uint8_t *)(buffer + byteNr + 1)) = (uint8_t) (value >> 8);
	*((uint8_t *)(buffer + byteNr))     = (uint8_t) (value);
}

void pack_add_u16(uint8_t *buffer, int byteNr, uint16_t value)  {
	*((uint8_t *)(buffer + byteNr + 1)) = (uint8_t) (value >> 8);
	*((uint8_t *)(buffer + byteNr))     = (uint8_t) (value);
}

void pack_add_32(uint8_t *buffer, int byteNr, int32_t value)  {
	*((uint8_t *)(buffer + byteNr + 3)) = (uint8_t) (value >> 24);
	*((uint8_t *)(buffer + byteNr + 2)) = (uint8_t) (value >> 16);
	*((uint8_t *)(buffer + byteNr + 1)) = (uint8_t) (value >>  8);
	*((uint8_t *)(buffer + byteNr))     = (uint8_t) (value);
}

void pack_add_u32(uint8_t *buffer, int byteNr, uint32_t value)  {
	*((uint8_t *)(buffer + byteNr + 3)) = (uint8_t) (value >> 24);
	*((uint8_t *)(buffer + byteNr + 2)) = (uint8_t) (value >> 16);
	*((uint8_t *)(buffer + byteNr + 1)) = (uint8_t) (value >>  8);
	*((uint8_t *)(buffer + byteNr))     = (uint8_t) (value);
}

void pack_add_64(uint8_t *buffer, int byteNr, int64_t value)  {
	*((uint8_t *)(buffer + byteNr + 7)) = (uint8_t) (value >> 56);
	*((uint8_t *)(buffer + byteNr + 6)) = (uint8_t) (value >> 48);
	*((uint8_t *)(buffer + byteNr + 5)) = (uint8_t) (value >> 40);
	*((uint8_t *)(buffer + byteNr + 4)) = (uint8_t) (value >> 32);
	*((uint8_t *)(buffer + byteNr + 3)) = (uint8_t) (value >> 24);
	*((uint8_t *)(buffer + byteNr + 2)) = (uint8_t) (value >> 16);
	*((uint8_t *)(buffer + byteNr + 1)) = (uint8_t) (value >>  8);
	*((uint8_t *)(buffer + byteNr))     = (uint8_t) (value);
}

void pack_add_u64(uint8_t *buffer, int byteNr, uint64_t value)  {
	*((uint8_t *)(buffer + byteNr + 7)) = (uint8_t) (value >> 56);
	*((uint8_t *)(buffer + byteNr + 6)) = (uint8_t) (value >> 48);
	*((uint8_t *)(buffer + byteNr + 5)) = (uint8_t) (value >> 40);
	*((uint8_t *)(buffer + byteNr + 4)) = (uint8_t) (value >> 32);
	*((uint8_t *)(buffer + byteNr + 3)) = (uint8_t) (value >> 24);
	*((uint8_t *)(buffer + byteNr + 2)) = (uint8_t) (value >> 16);
	*((uint8_t *)(buffer + byteNr + 1)) = (uint8_t) (value >>  8);
	*((uint8_t *)(buffer + byteNr))     = (uint8_t) (value);
}

void pack_add_flt(uint8_t *buffer, int byteNr, float value)  {
	*(buffer + byteNr + 3) = *(((uint8_t *) (&value)) + 3);
	*(buffer + byteNr + 2) = *(((uint8_t *) (&value)) + 2);
	*(buffer + byteNr + 1) = *(((uint8_t *) (&value)) + 1);
	*(buffer + byteNr)     = *((uint8_t *) (&value));
}

void pack_add_adr(uint8_t *buffer, int byteNr, uint32_t value)  {
	(*((uint8_t *)(buffer + byteNr + 3)) = (uint8_t) (value >> 24));
	(*((uint8_t *)(buffer + byteNr + 2)) = (uint8_t) (value >> 16));
	(*((uint8_t *)(buffer + byteNr + 1)) = (uint8_t) (value >>  8));
	(*((uint8_t *)(buffer + byteNr))     = (uint8_t) (value));
}

/**************************************************************
 * int8_t pack_get_8      (uint8_t *buffer, int byteNr)
 * uint8_t pack_get_u8   (uint8_t *buffer, int byteNr)
 * int16_t pack_get_16    (uint8_t *buffer, int byteNr)
 * uint16_t pack_get_u16  (uint8_t *buffer, int byteNr)
 * int32_t pack_get_32    (uint8_t *buffer, int byteNr)
 * uint32_t pack_get_u32  (uint8_t *buffer, int byteNr)
 * int64_t pack_get_64    (uint8_t *buffer, int byteNr)
 * uint64_t pack_get_u64  (uint8_t *buffer, int byteNr)
 * float pack_get_flt(uint8_t *buffer, int byteNr)
 * uint32_t pack_get_adr  (uint8_t *buffer, int byteNr)
 *
 * Functions will return parameter taken from buffer pointed
 * to by *(buffer + byteNr).
 *
 * Have Care!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Functions are for a CPU with little endian byte ordering,
 * like the NIOS II CPU. For a CPU with big endian byte
 * ordering you have to rewrite the functions.
 *************************************************************/
int8_t pack_get_8(uint8_t *buffer, int byteNr) {
	int8_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

uint8_t pack_get_u8(uint8_t *buffer, int byteNr) {
	uint8_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

int16_t pack_get_16(uint8_t *buffer, int byteNr) {
	int16_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+1) = *(buffer + byteNr + 1);
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

uint16_t pack_get_u16(uint8_t *buffer, int byteNr) {
	uint16_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+1) = *(buffer + byteNr + 1);
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

int32_t pack_get_32(uint8_t *buffer, int byteNr) {
	int32_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+3) = *(buffer + byteNr + 3);
	*(ptr+2) = *(buffer + byteNr + 2);
	*(ptr+1) = *(buffer + byteNr + 1);
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

uint32_t pack_get_u32(uint8_t *buffer, int byteNr) {
	uint32_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+3) = *(buffer + byteNr + 3);
	*(ptr+2) = *(buffer + byteNr + 2);
	*(ptr+1) = *(buffer + byteNr + 1);
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

int64_t pack_get_64(uint8_t *buffer, int byteNr) {
	int64_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+7) = *(buffer + byteNr + 7);
	*(ptr+6) = *(buffer + byteNr + 6);
	*(ptr+5) = *(buffer + byteNr + 5);
	*(ptr+4) = *(buffer + byteNr + 4);
	*(ptr+3) = *(buffer + byteNr + 3);
	*(ptr+2) = *(buffer + byteNr + 2);
	*(ptr+1) = *(buffer + byteNr + 1);
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

uint64_t pack_get_u64(uint8_t *buffer, int byteNr) {
	uint64_t retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+7) = *(buffer + byteNr + 7);
	*(ptr+6) = *(buffer + byteNr + 6);
	*(ptr+5) = *(buffer + byteNr + 5);
	*(ptr+4) = *(buffer + byteNr + 4);
	*(ptr+3) = *(buffer + byteNr + 3);
	*(ptr+2) = *(buffer + byteNr + 2);
	*(ptr+1) = *(buffer + byteNr + 1);
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

float pack_get_flt(uint8_t *buffer, int byteNr) {
	float retVal;
	uint8_t *ptr;

	ptr = (uint8_t *) &retVal;
	*(ptr+3) = *(buffer + byteNr + 3);
	*(ptr+2) = *(buffer + byteNr + 2);
	*(ptr+1) = *(buffer + byteNr + 1);
	*(ptr+0) = *(buffer + byteNr + 0);
	return(retVal);
}

uint32_t pack_get_adr(uint8_t *buffer, int byteNr) {
	return( (*((uint8_t *) (buffer + byteNr +3))) << 24 |
		(*((uint8_t *) (buffer + byteNr +2))) << 16 |
		(*((uint8_t *) (buffer + byteNr +1))) <<  8 |
		(*((uint8_t *) (buffer + byteNr))) );
}
