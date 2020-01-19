/*
  libvoxin API, v 1.4.6 copied from
  https://raw.githubusercontent.com/Oralux/libvoxin/2ad36d8f7c07ea80663c9b3524de443abdf76758/src/api/voxin.h

  Copyright: 2016-2020 Gilles Casse <gcasse@oralux.org>
  License: LGPL-2.1+ or MIT
 
*/
#ifndef VOXIN_H
#define VOXIN_H

#include <stdint.h>
#include "eci.h"

#define LIBVOXIN_VERSION_MAJOR 1
#define LIBVOXIN_VERSION_MINOR 4
#define LIBVOXIN_VERSION_PATCH 6

typedef enum {voxFemale, voxMale} voxGender;
typedef enum {voxAdult, voxChild, voxSenior} voxAge;

#define VOX_ECI_VOICES 22
#define VOX_RESERVED_VOICES 30
#define VOX_MAX_NB_OF_LANGUAGES (VOX_ECI_VOICES + VOX_RESERVED_VOICES)
#define VOX_STR_MAX 128
#define VOX_LAST_ECI_VOICE eciStandardFinnish

typedef struct {
  uint32_t id; // voice identifier, e.g.: 0x2d0002
  char name[VOX_STR_MAX]; // optional: 'Yelda',...
  char lang[VOX_STR_MAX]; // ietf sub tag, iso639-1, 2 letters code: 'en', 'tr',...
  char variant[VOX_STR_MAX]; // ietf sub tag, optional: 'scotland', 'CA',...
  uint32_t rate; // sample rate in Hertz: 11025, 22050
  uint32_t  size; // sample size e.g. 16 bits
  /* chanels = 1 */
  /* encoding = signed-integer PCM */
  char charset[VOX_STR_MAX]; // "UTF-8", "ISO-8859-1",...
  voxGender gender;
  voxAge age;
  char multilang[VOX_STR_MAX]; // optional, e.g. "en,fr"
  char quality[VOX_STR_MAX]; // optional, e.g. "embedded-compact"
  uint32_t tts_id;
} vox_t;

// voxGetVoices returns the list of available languages.
// This functions depreciates eciGetAvailableLanguages.
// eciGetAvailableLanguages behaves identically if the installed
// languages come from ECI.
// Otherwise, eciGetAvailableLanguages returns an unexpected language
// identifier.
//
// Return 0 if ok
int voxGetVoices(vox_t *list, unsigned int *nbVoices);

// convert the vox_t data to a string in the buffer supplied by the caller (up to len, 0 terminator included)
// return 0 if ok
int voxString(vox_t *v, char *s, size_t len);

#endif

