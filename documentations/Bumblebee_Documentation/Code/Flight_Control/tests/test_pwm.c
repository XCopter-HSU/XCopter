/*
 * This is the implementation of the pwm test module.
 */

//-----------------------Includes----------------------------------------------
#include "test_pwm.h"
#include "../drivers/b_pwmdriver.h"
#include <unistd.h>

//-----------------------Method Implementation---------------------------------
/*
 * Tests the pwm controller.
 */
void testPWM(enum PwmController controller) {
	printf("Maximal\n");
	PWMDriver_setSignalWidth(254, controller);
	scanf("%c");
	printf("Minimal\n");
	PWMDriver_setSignalWidth(1, controller);
	scanf("%c");

	printf("Low\n");
	PWMDriver_setSignalWidth(30, controller);
	scanf("%c");
	printf("Mid\n");
	PWMDriver_setSignalWidth(120, controller);
	scanf("%c");
	printf("High\n");
	PWMDriver_setSignalWidth(210, controller);
	scanf("%c");
	printf("Minimal\n");
	PWMDriver_setSignalWidth(1, controller);
	scanf("%c");

	printf("Stufen\n");
	int i = 0;
	for(i = 1; i < 255; i++) {
		printf("Running PWM with %f percent width!\n", (i/(float)255)*100);
		PWMDriver_setSignalWidth(i, controller);
		usleep(100000);
	}
	PWMDriver_setSignalWidth(0, controller);
}
