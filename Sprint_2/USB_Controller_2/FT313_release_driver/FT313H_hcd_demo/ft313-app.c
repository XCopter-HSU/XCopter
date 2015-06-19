#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <string.h>

#include "ft313_app.h"

int main(int argv, char *argc[])
{
	int handle, ret = 0;

	/*if there are arguments less than 2 */
	if (argv < 2)
	{
		printf("usage of application \n");
		printf("ft313 [PARAMETER] \n");
		printf("Ex: ./ft313 suspend/resume\n");
		printf("Parameter    Explanation\n");
		printf("------------------------\n");
		printf("suspend -put bus into suspend mode, supports remotewakeup,power of insertion\n");
		printf("resume -put bus into resume mode \n");

		printf("Press Enter Key to Continue \n");
		scanf("%c");
		return -1;
	}

	handle = open(devpath, O_RDWR);
	if (handle < 0)
	{
		printf("can not open the file %s\n", devpath);
		printf("Press Enter Key to Continue \n");
		scanf("%c");
		return handle;
	}

	if (strcmp(argc[1],"suspend") == 0) {
		ret = ioctl(handle, FT313_IOC_SUSPEND, NULL);
		if (ret < 0) {
			printf("FT313 Chip suspend fails\n");
		} else {
			printf("FT313 Chip suspend sucessfully\n");
		}

	} else if (strcmp(argc[1],"resume") == 0) {
		ret = ioctl(handle, FT313_IOC_RESUME, NULL);
		if (ret < 0) {
			printf("FT313 Chip resume fails\n");
		} else {
			printf("FT313 Chip resume sucessfully\n");
		}
	}  else if (strcmp(argc[1],"reset") == 0) {
		ret = ioctl(handle, FT313_IOC_RESET, NULL);
		if (ret < 0) {
			printf("FT313 Chip reset fails\n");
		} else {
			printf("FT313 Chip reset sucessfully\n");
		}
	} else {
		printf("Wrong parameter\n");
	}

	close(handle);
	return ret;
}
