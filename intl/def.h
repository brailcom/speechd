/* Common header file for robod server, clients and output drivers
 * CVS revision: $Id: def.h,v 1.5 2003-01-04 22:24:04 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

/* some constants common for speech server and client part */

#ifndef SPEECHD_DEF_I
	#define SPEECHD_DEF_I

#define SPEECH_PORT 9876

#define LOG_LEVEL 10

#define MSG(level,args...) { if (level <= LOG_LEVEL) printf(args); }

#define FATAL(msg) { printf("speechd [%s:%d]: "msg"\n", __FILE__, __LINE__); exit(EXIT_FAILURE); }

#define DIE(msg)   { perror("speechd: "msg); exit(EXIT_FAILURE); }


#endif
