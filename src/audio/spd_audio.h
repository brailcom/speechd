
#ifndef SPD_AUDIO
#define SPD_AUDIO

#include <flite/flite.h>

int spd_audio_play_wave(const cst_wave *w);
int spd_audio_open(const cst_wave *w);
void spd_audio_close();

int spd_audio_play_file(const char* filename);
int spd_audio_play_file_ogg(const char* filename);
int spd_audio_play_file_wav(const char* filename);

#endif /* SPD_AUDIO */
