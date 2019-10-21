#include <stdlib.h>
#include "debug.h"
#include <string.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

FILE *libvoxinDebugFile = NULL;
static enum libvoxinDebugLevel myDebugLevel = LV_ERROR_LEVEL;
FILE *libvoxinDebugText = NULL;
static size_t debugTextCount = 0; // number of bytes written to libvoxinDebugText
static int checkEnableCount = 0;

static int createDebugFile(const char *filename, FILE **fdi) {
  FILE *fd = NULL;
  mode_t old_mask;
  struct stat buf;
  
  if (!filename || !fdi)
    goto exit0;

  fd = *fdi;  
  if (fd) {
    fclose(fd);
  }
  fd = NULL;

  // the debug file must be read by the user only
  unlink(filename);
  old_mask = umask(0077);
  fd = fopen(filename, "w");  
  umask(old_mask);
  if (!fd)
    goto exit0;
  
  if (fstat(fileno(fd), &buf) || buf.st_mode & 0077) {
    goto exit0;
  }
  
  setbuf(fd, NULL);
  *fdi = fd;
  return 0;
  
 exit0:
  if (fd) {
	fclose(fd);
	fd = NULL;
  }
  if (fdi) {
	*fdi = NULL;
  }
  return 1;
}

static void deleteDebugFile(FILE **fdi) {
  if (!fdi)
	return;

  if (*fdi) {
    fclose(*fdi);
  }
  *fdi = NULL;
}

static void DebugFileInit()
{
  FILE *fd = NULL;
  char c;
#define MAX_FILENAME 40
  char filename[MAX_FILENAME+1];

  if (checkEnableCount)
	return;
  
  checkEnableCount = 1;
  
  char *home = getenv("HOME");
  if (!home)
	return;
  
  if (snprintf(filename, MAX_FILENAME, "%s/%s", home, ENABLE_LOG) >= MAX_FILENAME)
	return;
  
  fd = fopen(filename, "r");
  if (!fd)
    return;
  
  myDebugLevel = LV_DEBUG_LEVEL;
  if (fread(&c, 1, 1, fd)) {
    uint32_t i = atoi(&c);
    if (i <= LV_DEBUG_LEVEL)
      myDebugLevel = i;
  }
  fclose(fd);
  
  if (snprintf(filename, MAX_FILENAME, LIBVOXINLOG, getpid()) >= MAX_FILENAME)
	return;
  
  if (createDebugFile(filename, &libvoxinDebugFile))
	return;
  
  if (snprintf(filename, MAX_FILENAME, LIBVOXINLOG ".txt", getpid()) >= MAX_FILENAME)
	return;
  
  debugTextCount = 0;
  createDebugFile(filename, &libvoxinDebugText);
}

void libvoxinDebugFinish()
{
  deleteDebugFile(&libvoxinDebugFile);  
  debugTextCount = 0;
  deleteDebugFile(&libvoxinDebugText);
  checkEnableCount = 0;
}

int libvoxinDebugEnabled(enum libvoxinDebugLevel level)
{
	if (!libvoxinDebugFile) {
	  DebugFileInit();
	}

  return (libvoxinDebugFile && (level <= myDebugLevel)); 
}


void libvoxinDebugDisplayTime()
{
  struct timeval tv;
  if (!libvoxinDebugFile)
    DebugFileInit();

  if (!libvoxinDebugFile)
    return;
  
  gettimeofday(&tv, NULL);
  fprintf(libvoxinDebugFile, "%03ld.%06ld ", tv.tv_sec%1000, tv.tv_usec);
}


void libvoxinDebugDump(const char *label, uint8_t *buf, size_t size)
{
#define MAX_BUF_SIZE 1024 
  size_t i;
  char line[20];

  if (!buf || !label)
    return;

  if (size > MAX_BUF_SIZE)
    size = MAX_BUF_SIZE;

  if (!libvoxinDebugFile)
    DebugFileInit();
  
  if (!libvoxinDebugFile)
    return;
  
  memset(line ,0, sizeof(line));
  fprintf(libvoxinDebugFile, "\n-------------------\n");
  fprintf(libvoxinDebugFile, "\nDump %s", label);

  for (i=0; i<size; i++) {
    if (!(i%16)) {
      fprintf(libvoxinDebugFile, "  %s", line);
      memset(line, 0, sizeof(line));
      fprintf(libvoxinDebugFile, "\n%p  ", buf+i);
    }
    fprintf(libvoxinDebugFile, "%02x ", buf[i]);
    line[i%16] = isprint(buf[i]) ? buf[i] : '.';
  }

  fprintf(libvoxinDebugFile, "\n");
}


size_t libvoxinDebugTextWrite(const char *text, size_t len)
{
  ssize_t ret = 0;
  if (!libvoxinDebugText)
	return -1;
  ret = write (fileno(libvoxinDebugText), text, len);
  if (libvoxinDebugFile) {
	if (ret == -1) {
	  int err = errno;	
	  fprintf (libvoxinDebugFile, "%s: %s\n", __func__, strerror(err));
	} else {
	  fprintf (libvoxinDebugFile, "%s: text pos=%lu, len=%lu\n", __func__, (long unsigned int)debugTextCount, (long unsigned int)ret);
	  debugTextCount += ret;
	}
  }
  return debugTextCount;  
}

