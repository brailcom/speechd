#ifndef FDSETCONV_H
#define FDSETCONV_H

#include <stdio.h>
#include <string.h>
#include <speechd_types.h>

char* EVoice2str(EVoiceType voice);

EVoiceType str2EVoice(char* str);

char* EPunctMode2str(SPDPunctuation punct);

SPDPunctuation str2EPunctMode(char* str);

char* ESpellMode2str(SPDSpelling spell);

SPDSpelling str2ESpellMode(char* str);

char* ECapLetRecogn2str(SPDCapitalLetters recogn);

SPDCapitalLetters str2ECapLetRecogn(char* str);

EVoiceType str2intpriority(char* str);

#endif
