/* Header file for robod output modules
 * CVS revision: $Id: module.h,v 1.3 2002-07-18 17:52:37 hanke Exp $
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
