/* Header file for speechd output modules
 * CVS revision: $Id: module.h,v 1.1 2001-04-10 10:42:05 cerha Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

typedef struct {
   gchar    *name;
   gchar    *description;
   gchar    *filename;
   gint     (*write)    (const gchar *, gint);
   gint     (*stop)     (void);
   gint     (*pause)    (void);
   gint     (*close)    (void);
} OutputModule;

typedef OutputModule entrypoint (void);
