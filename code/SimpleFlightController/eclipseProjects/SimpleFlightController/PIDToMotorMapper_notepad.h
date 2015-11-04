#ifndef __PIDToMotorMapper__
#define __PIDToMotorMapper__

#include "confMapper.h"
#include "Driver_Motor.h"
#include "b_errorcodes.h"

int8_t motorQuadx[4];
int8_t motorQuadx[6];

int8_t mapToMotors(int16_t throttle, int16_t roll, int16_t pitch, int16_t yaw, int16_t yawDirection);

void writeToMotors();



#endif