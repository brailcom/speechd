

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <flite/flite.h>
#include <signal.h>

/* Voice */
extern cst_voice *register_cmu_us_kal();
cst_voice *spd_audio_flite_voice;
cst_audiodev *spd_audio_device = NULL;

int
spd_audio_play_wave(cst_wave *w)
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
spd_audio_read_wave(char *filename)
{
    cst_wave *new;
    new = malloc(1000000);
    cst_wave_load_riff(new, filename);
    return new;
}

int
spd_audio_open(cst_wave *w)
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

/*
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
*/
