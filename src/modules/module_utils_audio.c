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
 * $Id: module_utils_audio.c,v 1.2 2003-09-22 00:32:31 hanke Exp $
 */

#include "spd_audio.h"

static cst_wave *module_sample_wave = NULL;

static void module_cstwave_free(cst_wave* wave);

void
module_audio_output_child(TModuleDoublePipe dpipe, const size_t nothing)
{
    char *data;  
    cst_wave *wave;
    short *samples = NULL;
    int bytes;
    int num_samples = 0;
    int ret;
    int data_mode = 0;

    module_sigblockall();

    module_child_dp_init(dpipe);
    
    data = xmalloc(CHILD_SAMPLE_BUF_SIZE);

    ret = spd_audio_open(module_sample_wave);
    if (ret != 0){
        DBG("Can't open audio output!\n");
        module_child_dp_close(dpipe);
        exit(0);
    }

    DBG("Entering child loop\n");
    while(1){
        /* Read the waiting data */
        bytes = module_child_dp_read(dpipe, data, CHILD_SAMPLE_BUF_SIZE);
        DBG("child: Got %d bytes\n", bytes);
        if (bytes == 0){
            DBG("child: exiting, closing audio, pipes");
            spd_audio_close();
            module_child_dp_close(dpipe);
            DBG("child: good bye");
            exit(0);
        }

        /* Are we at the end? */
        if (bytes>=24){
            if (!strncmp(data+bytes-24,"\r\nOK_SPEECHD_DATA_SENT\r\n", 24)) data_mode = 2;
            if (!strncmp(data,"\r\nOK_SPEECHD_DATA_SENT\r\n", 24)) data_mode = 1;        
        }

        if ((data_mode == 1) || (data_mode == 2)){
            if (data_mode == 2){
                samples = module_add_samples(samples, (short*) data,
                                             bytes-24, &num_samples); 
            }

            DBG("child: End of data caught\n");
            wave = (cst_wave*) xmalloc(sizeof(cst_wave));
            wave->type = strdup("riff");
            wave->sample_rate = 0; /* We don't use sample rate here, it was significant
                                    when opening the device */
            wave->num_samples = num_samples;
            wave->num_channels = 1;
            wave->samples = samples;

            DBG("child: Sending data to audio output...\n");

            spd_audio_play_wave(wave);
            module_cstwave_free(wave);            

            DBG("child->parent: Ok, send next samples.");
            module_child_dp_write(dpipe, "C", 1);

            num_samples = 0;        
            samples = NULL;
            data_mode = 0;
        }else{
            samples = module_add_samples(samples, (short*) data,
                                         bytes, &num_samples);
        } 
    }        
}

static short *
module_add_samples(short* samples, short* data, size_t bytes, size_t *num_samples)
{
    int i;
    short *new_samples;
    static size_t allocated;

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

    return new_samples;
}

static int
module_parent_send_samples(TModuleDoublePipe dpipe, short* samples, size_t num_samples)
{
    size_t ret;
    size_t acc_ret = 0;

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
}

static void
module_cstwave_free(cst_wave* wave)
{
    if (wave != NULL){
        if (wave->samples != NULL) free(wave->samples);
        free(wave);
    }
}
