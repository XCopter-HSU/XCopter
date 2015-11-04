/*
 * The driver for the used PWM IP-Core that allows to vary the signal width.
 */

//-----------------------Includes----------------------------------------------
#include "Driver_PWM.h"
#include <system.h>
#include "../b_errorcodes.h"

//-----------------------Defines-----------------------------------------------
#define PWMDriver_WIDTHREG1 (*((volatile uint8_t *) 0x9000028))
#define PWMDriver_WIDTHREG2 (*((volatile uint8_t *) 0x9000074))
#define PWMDriver_WIDTHREG3 (*((volatile uint8_t *) 0x9000070))
#define PWMDriver_WIDTHREG4 (*((volatile uint8_t *) 0x900006c))
#define PWMDriver_WIDTHREG5 (*((volatile uint8_t *) 0x9000068))
#define PWMDriver_WIDTHREG6 (*((volatile uint8_t *) 0x9000064))
#define PWMDriver_WIDTHREG7 (*((volatile uint8_t *) 0x9000060))
#define PWMDriver_WIDTHREG8 (*((volatile uint8_t *) 0x900002c))

//-----------------------Method Implementation---------------------------------
/*
 * Function init initializes the PWM controller.
 */
int8_t PWMDriver_init() {
	PWMDriver_setSignalWidth(0, PWM_1);
	PWMDriver_setSignalWidth(0, PWM_2);
	PWMDriver_setSignalWidth(0, PWM_3);
	PWMDriver_setSignalWidth(0, PWM_4);
	PWMDriver_setSignalWidth(0, PWM_5);
	PWMDriver_setSignalWidth(0, PWM_6);
	PWMDriver_setSignalWidth(0, PWM_7);
	PWMDriver_setSignalWidth(0, PWM_8);
	return NO_ERR;
}

/*
 * Function setSignalWidthPercent initializes sets the signal width of the PWM signal.
 */
int8_t PWMDriver_setSignalWidthPercent(uint8_t widthValue,
		enum PwmController controller) {
	if (widthValue > 100 || widthValue < 0) {
		return ERR_PWM_ILLEGAL_RANGE;
	}
	widthValue = widthValue * ((float) 255 / 100);
	PWMDriver_setSignalWidth(widthValue, controller);
	return NO_ERR;
}

/*
 * Function setSignalWidthPercent initializes sets the signal width of the PWM signal.
 */
int8_t PWMDriver_setSignalWidth(uint8_t widthValue,
		enum PwmController controller) {
	if (widthValue > 255 || widthValue < 0) {
		return ERR_PWM_ILLEGAL_RANGE;
	}
	switch (controller) {
	case PWM_1:
		PWMDriver_WIDTHREG1 = widthValue;
		break;
	case PWM_2:
		PWMDriver_WIDTHREG2 = widthValue;
		break;
	case PWM_3:
		PWMDriver_WIDTHREG3 = widthValue;
		break;
	case PWM_4:
		PWMDriver_WIDTHREG4 = widthValue;
		break;
	case PWM_5:
		PWMDriver_WIDTHREG5 = widthValue;
		break;
	case PWM_6:
		PWMDriver_WIDTHREG6 = widthValue;
		break;
	case PWM_7:
		PWMDriver_WIDTHREG7 = widthValue;
		break;
	case PWM_8:
		PWMDriver_WIDTHREG8 = widthValue;
		break;
	}
	return NO_ERR;
}
