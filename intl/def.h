/* Common header file for speechd server, clients and output drivers
   by Tomas Cerha <cerha@brailcom.cz> */


/* we define some constants common for speech server and client part */
#define SPEECH_PORT 9876

/* here come some common macro definitions */
#define die(msg) { perror(msg); exit(1); }
