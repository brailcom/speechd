/* Common header file for robod server, clients and output drivers
 * CVS revision: $Id: def.h,v 1.4 2002-12-16 01:46:29 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

/* some constants common for speech server and client part */

#ifndef SPEECHD_DEF_I
	#define SPEECHD_DEF_I

#define SPEECH_PORT 9876

#define LOG_LEVEL 3

#define MSG(level,args...) { if (level >= LOG_LEVEL) printf(args); }

#define FATAL(msg) { printf("speechd: "msg"\n"); exit(EXIT_FAILURE); }

#define DIE(msg)   { perror("speechd: "msg); exit(EXIT_FAILURE); }


#endif
