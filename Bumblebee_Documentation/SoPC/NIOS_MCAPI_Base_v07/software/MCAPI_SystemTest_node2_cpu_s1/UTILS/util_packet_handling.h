/*************************************************************
 * file: util_packet_handling.h
 *
 * DESCRIPTION:
 * Prototype definitions of functions given in
 * util_packet_handling.c
 *
 * EDITION HISTORY:
 * 2013-12-09: getting started - ms (M. Strahnen)
 *************************************************************/

#ifndef UTIL_PACKET_HANDLING_H_
#define UTIL_PACKET_HANDLING_H_

#include <stdint.h>	// necessary for intX_t and uintX_t data types

extern void pack_add_8(uint8_t *buffer, int byteNr, int8_t value);
extern void pack_add_u8(uint8_t *buffer, int byteNr, uint8_t value);
extern void pack_add_16(uint8_t *buffer, int byteNr, int16_t value);
extern void pack_add_u16(uint8_t *buffer, int byteNr, uint16_t value);
extern void pack_add_32(uint8_t *buffer, int byteNr, int32_t value);
extern void pack_add_u32(uint8_t *buffer, int byteNr, uint32_t value);
extern void pack_add_64(uint8_t *buffer, int byteNr, int64_t value);
extern void pack_add_u64(uint8_t *buffer, int byteNr, uint64_t value);
extern void pack_add_flt(uint8_t *buffer, int byteNr, float value);
extern void pack_add_adr(uint8_t *buffer, int byteNr, uint32_t value);

extern int8_t pack_get_8(uint8_t *buffer, int byteNr);
extern uint8_t pack_get_u8(uint8_t *buffer, int byteNr);
extern int16_t pack_get_16(uint8_t *buffer, int byteNr);
extern uint16_t pack_get_u16(uint8_t *buffer, int byteNr);
extern int32_t pack_get_32(uint8_t *buffer, int byteNr);
extern uint32_t pack_get_u32(uint8_t *buffer, int byteNr);
extern int64_t pack_get_64(uint8_t *buffer, int byteNr);
extern uint64_t pack_get_u64(uint8_t *buffer, int byteNr);
extern float pack_get_flt(uint8_t *buffer, int byteNr);
extern uint32_t pack_get_adr(uint8_t *buffer, int byteNr);

#endif /* UTIL_PACKET_HANDLING_H_ */
