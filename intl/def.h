/* Common header file for speechd server, clients and output drivers
 * CVS revision: $Id: def.h,v 1.2 2001-04-10 10:42:05 cerha Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

/* some constants common for speech server and client part */
#define SPEECH_PORT 9876

#define LOG_LEVEL = 3

#define MSG(level,message,args...) if (level >= LOG_LEVEL) printf(message,args...)
