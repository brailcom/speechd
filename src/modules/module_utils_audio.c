/*
 * module_utils_audio.c - Audio output functionality for output modules
 * 
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: module_utils_audio.c,v 1.9 2004-06-28 08:09:50 hanke Exp $
 */

#include "spd_audio.h"

static void module_cstwave_free(cst_wave* wave);
static short* module_add_samples(short* samples, short* data, size_t bytes, 
                                 size_t *num_samples, int *end_of_data);

void
module_audio_output_child(TModuleDoublePipe dpipe, const size_t nothing)
{
    char *data;  
    cst_wave *wave;
    short *samples = NULL;
    int bytes;
    int num_samples = 0;
    int ret;
    int data_mode = -1;
    int bitrate = 0, volume = 0; /* It will get initialized in the first run  */
    char *tail_ptr;
    int eod;

    module_sigblockall();

    module_child_dp_init(dpipe);
    
    data = xmalloc(CHILD_SAMPLE_BUF_SIZE);

    DBG("Entering child loop\n");
    while(1){

        if (data_mode == -1){
            DBG("child: Checking for header.");
            bytes = module_child_dp_read(dpipe, data, 10);
            if (bytes != 0){
                if (bytes != 10){
                    DBG("child: Missing header of send data! (FATAL ERROR)");
                    exit(1); /* Missing header */
                }
                bitrate = strtol(data, &tail_ptr, 10);
                if(tail_ptr == data){
                    DBG("child: Invalid header of send data! (FATAL ERROR) [%s]", data);
                    exit(1);
                }          
            }else exit(1);

            bytes = module_child_dp_read(dpipe, data, 5);
            if (bytes != 0){
                if (bytes != 5){
                    DBG("child: Missing header of send data! (FATAL ERROR)");
                    exit(1); /* Missing header */
                }
                volume = strtol(data, &tail_ptr, 10);
                if(tail_ptr == data){
                    DBG("child: Invalid header of send data 2! (FATAL ERROR) [%s]", data);
                    exit(1);
                }          
            }else exit(1);
	    data_mode = 0;
        }

        /* Read the waiting data */
        bytes = module_child_dp_read(dpipe, data, CHILD_SAMPLE_BUF_SIZE);
        DBG("child: Got %d bytes\n", bytes);
        if (bytes == 0){
            DBG("child: exiting, closing audio, pipes");
            module_child_dp_close(dpipe);
            DBG("child: good bye");
            exit(0);
        }

        samples = module_add_samples(samples, (short*) data,
                                     bytes, &num_samples, &eod); 
        if (eod) data_mode = 1;

        if ((data_mode == 1)){
            DBG("child: End of data caught\n");
            wave = (cst_wave*) xmalloc(sizeof(cst_wave));
            wave->type = strdup("riff");
            wave->sample_rate = bitrate;
            wave->num_samples = num_samples;
            wave->num_channels = 1;
            wave->samples = samples;

            DBG("child: Opening audio...");                
            ret = spd_audio_open(wave);
            if (ret != 0){
                DBG("child: Can't open audio device!");
                exit(1);
            }

	    spd_audio_set_volume((volume-100)/2);

            DBG("child: Sending data to audio output...\n");            
            spd_audio_play_wave(wave);
            module_cstwave_free(wave);            

            DBG("child->parent: Ok, send next samples.");
            module_child_dp_write(dpipe, "C", 1);

            spd_audio_close();

            num_samples = 0;        
            samples = NULL;
            data_mode = -1;
        }
    }        
}

static short *
module_add_samples(short* samples, short* data, size_t bytes,
                   size_t* num_samples, int *end_of_data)
{
    int i;
    short *new_samples;
    static size_t allocated;

    *end_of_data = 0;

    if (samples == NULL) *num_samples = 0;
    if (*num_samples == 0){
        allocated = CHILD_SAMPLE_BUF_SIZE;
        new_samples = (short*) xmalloc(CHILD_SAMPLE_BUF_SIZE);        
    }else{
        new_samples = samples;
    }

    if (*num_samples * sizeof(short) + bytes > allocated){
        allocated *= 2;
        new_samples = (short*) xrealloc(new_samples, allocated);
    }

    for(i=0; i <= (bytes/sizeof(short)) - 1; i++){
        new_samples[*num_samples] = data[i];
        (*num_samples)++;
    }

    if((*num_samples)*sizeof(short) >= (24*sizeof(char))){
        char *p;
        p = ((char*) new_samples) + ((*num_samples) * sizeof(short)) - 24*sizeof(char);
        if (!memcmp(p, "\r\nOK_SPEECHD_DATA_SENT\r\n", 24 * sizeof(char))){
            DBG("End of data stream caught");
            *num_samples -= 24;
            *end_of_data = 1;
        }
    }

    return new_samples;
}

static int
module_parent_send_samples(TModuleDoublePipe dpipe, short* samples, size_t num_samples,
			   int bitrate, int volume)
{
    size_t ret;
    size_t acc_ret = 0;
    char param_str[10];

    snprintf(param_str, 15, "%-10d%-9d\n", bitrate, volume);
    ret = module_parent_dp_write(dpipe, param_str, 15);   
    if (ret == -1){
        DBG("parent: Error in sending bitrate information to child:\n   %s\n",
            strerror(errno));
        return -1;
    }

    while(acc_ret < num_samples * sizeof(short)){
        ret = module_parent_dp_write(dpipe, (char*) samples, 
                                     num_samples * sizeof(short) - acc_ret);
                
        if (ret == -1){
            DBG("parent: Error in sending data to child:\n   %s\n",
                strerror(errno));
            return -1;
        }
        acc_ret += ret;
    }
    return 0;
}

static void
module_cstwave_free(cst_wave* wave)
{
    if (wave != NULL){
        if (wave->samples != NULL) free(wave->samples);
        free(wave);
    }
}
