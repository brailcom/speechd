/* Header file for speechd output modules
 * CVS revision: $Id: module.h,v 1.4 2002-12-08 20:16:22 hanke Exp $
 * Author: Tomas Cerha <cerha@brailcom.cz> */

typedef struct {
   gchar    *name;
   gchar    *description;
   gchar    *filename;
   gint     (*write)    (const gchar *, gint, void*);
   gint     (*stop)     (void);
   gint     (*pause)    (void);
   gint     (*close)    (void);
   gint     (*is_speaking) (void);
} OutputModule;

typedef OutputModule entrypoint (void);

#define BUF_SIZE 4096
