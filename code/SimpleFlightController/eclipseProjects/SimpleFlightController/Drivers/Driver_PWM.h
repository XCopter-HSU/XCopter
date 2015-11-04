/*
 * This is a driver for the pwm controllers.
 */

#ifndef B_PWMDRIVER_H_
#define B_PWMDRIVER_H_

//-----------------------Includes----------------------------------------------
#include "../stdint.h"

//-----------------------Enum for PWM selection--------------------------------
enum PwmController {
	PWM_1, PWM_2, PWM_3, PWM_4, PWM_5, PWM_6, PWM_7, PWM_8
};

//-----------------------Method Declaration------------------------------------
/**
 * Function init initializes the PWM controller.
 * <p>
 * You should call this function before you first use the PWM driver.
 * It sets all PWM controllers to minimal signal width.
 *
 * @return	NO_ERR	If everything is fine.
 */
int8_t PWMDriver_init();

/**
 * Function setSignalWidthPercent initializes sets the signal width of the
 * PWM signal.
 * <p>
 * The function sets the width of the PWM signal and expects a range of 0
 * to 100 (percent). 0 produces a low signal and 100 a high signal.
 *
 * @param widthValue The width of the PWM signal.
 * @param controller The PWM controller that shall be set.
 * @return 	ERR_PWM_ILLEGAL_RANGE	If the range does not fit.
 * 			NO_ERR					If everything is fine.
 */
int8_t PWMDriver_setSignalWidthPercent(uint8_t widthValue,
		enum PwmController controller);

/**
 * Function setSignalWidthPercent initializes sets the signal width of the
 * PWM signal.
 * <p>
 * The function sets the width of the PWM signal and expects a range of 0
 * to 255. 0 produces a low signal and 255 a high signal.
 * @param widthValue The width of the PWM signal.
 * @param controller The PWM controller that shall be set.
 *
 * @return 	ERR_PWM_ILLEGAL_RANGE	If the range does not fit.
 * 			NO_ERR					If everything is fine.
 */
int8_t PWMDriver_setSignalWidth(uint8_t widthValue,
		enum PwmController controller);

#endif
