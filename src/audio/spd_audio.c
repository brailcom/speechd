

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <flite/flite.h>
#include <signal.h>

#include "spd_audio.h"

/* Voice */
extern cst_voice *register_cmu_us_kal();
cst_voice *spd_audio_flite_voice;
cst_audiodev *spd_audio_device = NULL;

int
spd_audio_play_wave(const cst_wave *w)
{
    int i, r, n;

    if (w == NULL){
        fprintf(stderr, "Passed is NULL\n");
        return -1;
    }

    if (spd_audio_device == NULL){
        fprintf(stderr, "Audio device not initialized.");
        return -1;
    }

    for (i=0; i < w->num_samples; i += r/2){
        if (w->num_samples > i+CST_AUDIOBUFFSIZE){
            n = CST_AUDIOBUFFSIZE;
        }else{
            n = w->num_samples-i;
        }

        r = audio_write(spd_audio_device, &w->samples[i], n*2);
        if (r <= 0){
		/* couldn't make it to the end */
            break;
        }
    }

    return 0;
}

cst_wave *
spd_audio_read_wave(const char *filename)
{
    cst_wave *new;
    new = (cst_wave*) malloc(sizeof(cst_wave));
    if (new == NULL){
        printf("Not enough memory");
        exit(1);
    }
    if (cst_wave_load_riff(new, filename) == -1) return NULL;   

    return new;
}

int
spd_audio_play_file_wav(const char* filename)
{
    cst_wave *wave;
    wave = (cst_wave*) spd_audio_read_wave(filename);
    if (wave == NULL) return -1;

    spd_audio_open(wave);
    spd_audio_play_wave(wave);
    spd_audio_close(wave);

    return 0;
}

int
spd_audio_play_file_ogg(const char* filename)
{
    char *cmd;
    /* TODO: This should be done better (using libvorbis?) ... */
    cmd = g_strdup_printf("ogg123 -d oss %s 2> /dev/null", filename);
    return system(cmd);
}

int
spd_audio_play_file(const char* filename)
{
    /* NOTE: I've no idea why the g_str_has_suffix(),
       as defined in documentation, is actually missing
       in libglib-2.0.so ?! */

    if(g_pattern_match_simple("*.wav*", filename))
        return spd_audio_play_file_wav(filename);
    else if (g_pattern_match_simple("*.ogg*", filename))
        return spd_audio_play_file_ogg(filename);
    else{
        fprintf(stderr,"Not matched!\n");
        return -1;
    }
}

int
spd_audio_open(const cst_wave *w)
{
    if (w == NULL){
        spd_audio_device = NULL;
        return -1;
    }
    if ((spd_audio_device = audio_open(w->sample_rate, w->num_channels,
                         CST_AUDIO_LINEAR16)) == NULL){
        return CST_ERROR_FORMAT;
    }
    return 0;
}

void
spd_audio_close()
{
    if (spd_audio_device != NULL)
        audio_close(spd_audio_device);

    spd_audio_device = NULL;
}

#if 0
int
main()
{
    cst_wave *wave1, *wave2, *wave3;
    printf("hello...\n");
    flite_init();
    flite_voice = register_cmu_us_kal();
    if (flite_voice == NULL)
        printf("Couldn't register the basic kal voice.\n"); 

    wave1 = flite_text_to_wave("c", flite_voice);
    wave2 = flite_text_to_wave("b", flite_voice);
    wave3 = flite_text_to_wave("a", flite_voice);

    spd_audio_open(wave1);

    spd_audio_play(wave1);
    spd_audio_play(wave2);
    spd_audio_play(wave3);

    spd_audio_close();

}	
#endif
