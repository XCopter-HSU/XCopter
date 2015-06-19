#ifndef __FT313_APP_H__
#define __FT313_APP_H__

#ifndef __KERNEL__
static char devpath[] = "./ft313-dev";
#endif
#ifdef USE_UDEV
#define DEVICE_NAME "ft313h"
#define CLASS_NAME "ftdi"
#else
#define FT313_MAJOR		245
#endif

#define FT313_IOC_MAGIC		'F'

#define FT313_IOC_SUSPEND	_IO(FT313_IOC_MAGIC, 1)
#define FT313_IOC_RESUME	_IO(FT313_IOC_MAGIC, 2)
#define FT313_IOC_RESET		_IO(FT313_IOC_MAGIC, 3)

#define FT313_IOC_MAXNR		3

#endif //__FT313_APP_H__
