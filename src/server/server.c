/* Speechd server functions
 * CVS revision: $Id: server.c,v 1.1 2001-04-10 10:42:05 cerha Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

#include "speechd.h"

#define BUF_SIZE 4096

int serve(int fd, GHashTable *output_modules) {
   int bytes;
   char buf[BUF_SIZE];
   OutputModule *output;
   char *reply;

   /* choose proper output module from given associative array */
   output = g_hash_table_lookup(output_modules, "mluvitko");

   /* read data from socket into internal buffer */
   bytes = read(fd, buf, BUF_SIZE);
   MSG(3,"    read %d bytes from client on fd %d\n", bytes, fd);

   /* write data from buffer to output module */
   (*output->write) (buf, bytes);

   /* send a reply to the socket */
   reply = "OK";
   write(fd, reply, strlen(reply));

   return 0;
}
