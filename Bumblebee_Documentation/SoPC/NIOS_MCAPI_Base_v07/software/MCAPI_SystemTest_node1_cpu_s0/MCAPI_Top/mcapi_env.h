/**************************************************************
 * FILE:                 mcapi_env.h
 *
 * DESCRIPTION:
 * Contains everything necessary to specify or describe the
 * environment in which MCAPI is running. Environment
 * description contains for example the specification of the
 * operating, may contain the specification of this CPU's
 * endianess, and so on.
 *
 * EDITION HISTRORY:
 * 2014-08-11: getting started
 *************************************************************/

#ifndef MCAPI_ENV_H_
#define MCAPI_ENV_H_

/* Define the operating system here. The definition is used in
 * several files to include operating system specific code */
#define	UCOSII	// include code for uC/OS-II operating system
//#define	LINUX	// include code for Linux operating system

/* Define the supporting PH layers here. The definitions are used
 * NS_layer software. */
//#define	SOCK
#define FIFO

#endif /* MCAPI_ENV_H_ */
