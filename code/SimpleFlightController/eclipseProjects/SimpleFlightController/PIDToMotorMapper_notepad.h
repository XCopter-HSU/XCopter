#ifndef __PIDToMotorMapper__
#define __PIDToMotorMapper__


int8_t motor[5];

int8_t mapToMotors(int16_t throttle, int16_t roll, int16_t pitch, int16_t yaw, int16_t yawDirection);

int8_t writeToMotors();



#endif