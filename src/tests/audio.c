
#include "spd_audio.h"

int
main(int argc, char *argv[])
{
    char *filename;
    cst_wave *wave;

    if (argc == 2){
        filename = strdup(argv[1]);
    }else{
        printf("Invalid arguments.\n"
               "Usage %s filename.wav\n", argv[0]);
        exit(1);
    }
    
    printf("Ok, playing file %s ...\n", filename);
    spd_audio_play_file(filename);
    printf("Done.\n");
}
