/* Common header file for robod server, clients and output drivers
 * CVS revision: $Id: def.h,v 1.3 2002-07-18 17:52:37 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

/* some constants common for speech server and client part */
#define SPEECH_PORT 9876

#define LOG_LEVEL = 3

#define MSG(level,message,args...) if (level >= LOG_LEVEL) printf(message,args...)
