#include "PIDToMotorMapper.h"


 
#define PIDMIX(X,Y,Z) throttle + roll *X + pitch *Y + yawDirection * yaw *Z    // YawDirection notwendig?????


//Maps the three values of the pids to each motor and computes the the PWM- signal for the motors.
int8_t mapToMotors(int16_t throttle, int16_t roll, int16_t pitch, int16_t yaw, int16_t yawDirection){
	int8_t i = 0;
	
	#if defined( QUADX )
		motorQuadx[0] = PIDMIX(-1,+1,-1); //REAR_R
		motorQuadx[1] = PIDMIX(-1,-1,+1); //FRONT_R
		motorQuadx[2] = PIDMIX(+1,+1,+1); //REAR_L
		motorQuadx[3] = PIDMIX(+1,-1,-1); //FRONT_L	
		
		//Compute PWM- signal
		for(i; i < sizeof(motorQuadx); i++){
			motorQuadx[i] = motorQuadx[i]//TODO *+-/.....PWM range berechnen 						----TODO----
		}
		
	/*#elif defined ( HEX )
		motor[0] = PIDMIX(-1,+1,-1);															//	----TODO----
		motor[1] = PIDMIX(-1,-1,+1);
		motor[2] = PIDMIX(+1,+1,+1); 
		motor[3] = PIDMIX(+1,-1,-1); 
		motor[4] = PIDMIX(+1,-1,-1);
		motor[5] = PIDMIX(+1,-1,-1);*/		
	 #endif

	return NO_ERR;
 }
 

//Writes the PWM- signal to each motor. 
void writeToMotors(){
	
	int16_t err = NO_ERR;	
	
		
	#if defined ( QUADX )
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Front_Left);							// ----TODO---- speed setzen und logerror schreiben
		if(err != NO_ERR)
			//logerror(err)
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Front_Right);
		if(err != NO_ERR)
			//logerror(err)
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Back_Left);
		if(err != NO_ERR)
			//logerror(err)
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Back_Right);
		if(err != NO_ERR)
			//logerror(err)
		
	#elif defined ( HEX )
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Front_Left);
		if(err != NO_ERR)
			//logerror(err)
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Front_Right);
		if(err != NO_ERR)
			//logerror(err)
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Middle_Left);
		if(err != NO_ERR)
			//logerror(err)
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Middle_Right);	
		if(err != NO_ERR)
			//logerror(err)		
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Back_Left);
		if(err != NO_ERR)
			//logerror(err)
		err = MotorDriver_setSpeed(uint8_t speed, Motor_Back_Right);
		if(err != NO_ERR)
			//logerror(err)	
	#endif
}