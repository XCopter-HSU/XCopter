#include "PIDToMotorMapper.h"


#define PWM_OBERGRENZE = 250		// ACHTUNG !!!!!!		Werte für Obergrenze und Untergrenze sind momentan nur Beispielwerte    !!!!!
#define PWM_UNTERGRENZE = 10 
#define PIDMIX(X,Y,Z) throttle + roll *X + pitch *Y + yawDirection * yaw *Z


//Maps the three values of the pids to each motor. You can define if you
int8_t mapToMotors(int16_t throttle, int16_t roll, int16_t pitch, int16_t yaw, int16_t yawDirection){
 
 #if defined( QUADX )
    motor[0] = PIDMIX(-1,+1,-1); //REAR_R
    motor[1] = PIDMIX(-1,-1,+1); //FRONT_R
    motor[2] = PIDMIX(+1,+1,+1); //REAR_L
    motor[3] = PIDMIX(+1,-1,-1); //FRONT_L	
	
/*#elif defined ( HEX )
	motor[0] = PIDMIX(-1,+1,-1);	// ACHTUNG !!!!!!		Sind noch nicht die richtigen Werte    !!!!!
    motor[1] = PIDMIX(-1,-1,+1);
    motor[2] = PIDMIX(+1,+1,+1); 
    motor[3] = PIDMIX(+1,-1,-1); 
	motor[4] = PIDMIX(+1,-1,-1);
	motor[5] = PIDMIX(+1,-1,-1);*/		
#endif
 }
 

//Computes the pwm Signal for each motor and write it to the motors 
int8_t writeToMotors(){

//TODO	
								
}