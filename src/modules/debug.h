#ifndef __DEBUG_H_
#define __DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
  //#include "msg.h"

  
  // log enabled if this file exits under $HOME
#define ENABLE_LOG "libvoxin.ok"

  // log level; first byte equals to a digit in DebugLevel (default 
#define LIBVOXINLOG "/tmp/libvoxin-sd.log.%d"

  enum libvoxinDebugLevel {LV_ERROR_LEVEL=0, LV_INFO_LEVEL=1, LV_DEBUG_LEVEL=2, LV_LOG_DEFAULT=LV_ERROR_LEVEL};

#define log(level,fmt,...) if (libvoxinDebugEnabled(level)) {libvoxinDebugDisplayTime(); fprintf (libvoxinDebugFile, "(sd_voxin) %s: " fmt "\n", __func__, ##__VA_ARGS__);}
#define log_bytes(level,label,bytes) if (libvoxinDebugEnabled(level) && label && bytes && bytes->b) { \
  libvoxinDebugDisplayTime(); \
  fprintf (libvoxinDebugFile, "%s: %s", __func__, label); /* unbuffered fprintf */ \
  write (fileno(libvoxinDebugFile), bytes->b, bytes->len); \
}
#define err(fmt,...) log(LV_ERROR_LEVEL, fmt, ##__VA_ARGS__)
#define msg(fmt,...) log(LV_INFO_LEVEL, fmt, ##__VA_ARGS__)
#define dbg(fmt,...) log(LV_DEBUG_LEVEL, fmt, ##__VA_ARGS__)
#define dbgText(text, len) if (libvoxinDebugEnabled(LV_DEBUG_LEVEL)) {libvoxinDebugTextWrite(text, len);}
  // display msg_bytes_t
#define dbg_bytes(label,bytes) {log_bytes(LV_DEBUG_LEVEL, label, bytes)}

#define ENTER() dbg("ENTER")
#define LEAVE() dbg("LEAVE")

  // compilation error if condition is not fullfilled (inspired from
  // BUILD_BUG_ON, linux kernel).
#define BUILD_ASSERT(condition) ((void)sizeof(char[(condition)?1:-1]))

  extern int libvoxinDebugEnabled(enum libvoxinDebugLevel level);
  extern void libvoxinDebugDisplayTime();
  extern size_t libvoxinDebugTextWrite(const char *text, size_t len);
  extern void libvoxinDebugDump(const char *label, uint8_t *buf, size_t size);
  extern void libvoxinDebugFinish();
  extern FILE *libvoxinDebugFile;  
  extern FILE *libvoxinDebugText;  

#ifdef __cplusplus
}
#endif

#endif

