
/*
 * alsa.c -- The Advanced Linux Sound System backend for Speech Dispatcher
 *
 * Copyright (C) 2004 Brailcom, o.p.s.
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
 * $Id: alsa.c,v 1.4 2005-07-02 12:15:44 hanke Exp $
 */

#ifndef timersub
#define	timersub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while (0)
#endif

/* Put a message into the logfile (stderr) */
#define MSG(arg...) \
{ \
    time_t t; \
    struct timeval tv; \
    char *tstr; \
    t = time(NULL); \
    tstr = strdup(ctime(&t)); \
    tstr[strlen(tstr)-1] = 0; \
    gettimeofday(&tv,NULL); \
    fprintf(stderr," %s [%d]",tstr, (int) tv.tv_usec); \
    fprintf(stderr," ALSA: "); \
    fprintf(stderr,arg); \
    fprintf(stderr,"\n"); \
    fflush(stderr); \
 }

/* I/O error handler */
int
xrun(AudioID *id)
{
    snd_pcm_status_t *status;
    int res;

    if (id == NULL) return -1;

    pthread_mutex_lock(&id->pcm_mutex);
    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(id->pcm, status))<0) {
        MSG("status error: %s", snd_strerror(res));
        pthread_mutex_unlock(&id->pcm_mutex);
        return -1;
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        struct timeval now, diff, tstamp;
        gettimeofday(&now, 0);
        snd_pcm_status_get_trigger_tstamp(status, &tstamp);
        timersub(&now, &tstamp, &diff);
        MSG("underrun!!! (at least %.3f ms long)",
            diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
        if ((res = snd_pcm_prepare(id->pcm))<0) {
            MSG("xrun: prepare error: %s", snd_strerror(res));
            pthread_mutex_unlock(&id->pcm_mutex);
            return -1;
        }
        pthread_mutex_unlock(&id->pcm_mutex);
        return 0;		/* ok, data should be accepted again */
    }
    MSG("read/write error, state = %s",
        snd_pcm_state_name(snd_pcm_status_get_state(status)));
    
    pthread_mutex_unlock(&id->pcm_mutex);
    return -1;
}

/* I/O suspend handler */
int
suspend(AudioID *id)
{
    int res;

    if (id == NULL) return -1;

    while ((res = snd_pcm_resume(id->pcm)) == -EAGAIN)
        sleep(1);	/* wait until suspend flag is released */
    pthread_mutex_lock(&id->pcm_mutex);
    if (res < 0) {
        if ((res = snd_pcm_prepare(id->pcm)) < 0) {
            MSG("suspend: prepare error: %s", snd_strerror(res));
            pthread_mutex_unlock(&id->pcm_mutex);
            return -1;
        }
    }
    pthread_mutex_unlock(&id->pcm_mutex);
    return 0;
}

/* 
 Open the device so that it's ready for playing on the default device. Internal
 function used by the public alsa_open.

 TODO: I don't know how to put dmix into this. Maybe the device ``default'' is
 not the right one?
*/
int
_alsa_open(AudioID *id, char *device)
{
    int err;

    assert (device);

    /* Open the device */
    if ((err = snd_pcm_open (&id->pcm, device,
			     SND_PCM_STREAM_PLAYBACK, 0)) < 0) { 
	MSG("Cannot open audio device (%s)",
		 snd_strerror (err));
	return -1;
    }
   
    /* Allocate space for hw_params (description of the sound parameters) */
    if ((err = snd_pcm_hw_params_malloc (&id->hw_params)) < 0) {
	MSG("Cannot allocate hardware parameter structure (%s)",
		 snd_strerror (err));
	return -1;
    }

    /* Initialize hw_params on our pcm */
    if ((err = snd_pcm_hw_params_any (id->pcm, id->hw_params)) < 0) {
	MSG("Cannot initialize hardware parameter structure (%s)",
		 snd_strerror (err));
	return -1;
    }

    return 0;
}

/* 
 Close the device. Internal function used by public alsa_close. 
*/

int
_alsa_close(AudioID *id)
{
    int err;

    if ((err = snd_pcm_close (id->pcm)) < 0) {
	MSG("Cannot close ALSA device (%s)\n",
		 snd_strerror (err));
	return -1;
    }
    return 0;
}


/*
 Open ALSA for playback.

 No parameters are passed in pars (*pars[0] == NULL).
*/
int
alsa_open(AudioID *id, void **pars)
{
    int ret;

    if (id == NULL) return 0;

    MSG("Opened succesfully");
    if (pars[0] == NULL) return -1;

    ret = _alsa_open(id, pars[0]);
    if (ret){
	MSG("Cannot initialize Alsa device '%s'", pars[0]);
	return -1;
    }

    pthread_mutex_init(&id->pcm_mutex, NULL);

    return 0; 
}

/* 
 Play the track _track_ (see spd_audio.h) using the id->pcm device and
 id-hw_params parameters. This is a blocking function, however, it's possible
 to interrupt playing from a different thread with alsa_stop(). alsa_play
 returns after and immediatelly after the whole sound was played on the
 speakers.
*/
int
alsa_play(AudioID *id, AudioTrack track)
{
    int ret;
    snd_pcm_format_t format;
    int channels;
    int bytes_per_sample;
    int num_bytes;
    int frames;
    signed short* output_samples;

    int bytes;
    float real_volume;
    int i;
    
    int err;

    snd_pcm_uframes_t framecount;

    AudioTrack track_volume;

    MSG("alsa_play called");

    if (id == NULL) return -1;

    /* Is it not an empty track? */
    if (track.samples == NULL) return 0;

    /* pcm_mutex: Other threads aren't allowed to change the state */
    pthread_mutex_lock(&id->pcm_mutex);
    /* Ensure we are in the right state */
    snd_pcm_state_t state = snd_pcm_state(id->pcm);
    MSG("pcm state: %s", snd_pcm_state_name(state));
    if ((snd_pcm_state(id->pcm) != SND_PCM_STATE_OPEN)
	&& (snd_pcm_state(id->pcm) != SND_PCM_STATE_SETUP))
	{
	    /* If draining is under way after alsa_stop, there is probably no
	       other option than to wait. */
	    while(snd_pcm_state(id->pcm) == SND_PCM_STATE_DRAINING) usleep(100);
	}

    /* Choose the correct format */
    if (track.bits == 16){
	/* TODO: Fix the endian issue since Alsa doesn't support native endian. */
	format = SND_PCM_FORMAT_S16_LE;
	bytes_per_sample = 2;
    }else if (track.bits == 8){
	bytes_per_sample = 1;
	format = SND_PCM_FORMAT_S8;
    }else{
	MSG("Unrecognized sound data format, track.bits = %d", track.bits);	
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }

    /* Set access mode, bitrate, sample rate and channels */
    MSG("setting access type to INTERLEAVED");
    if ((err = snd_pcm_hw_params_set_access (id->pcm, id->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
	MSG("Cannot set access type (%s)",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }
    
    MSG("setting sample format to %s", snd_pcm_format_name(format));
    if ((err = snd_pcm_hw_params_set_format (id->pcm, id->hw_params, format)) < 0) {
	MSG("Cannot set sample format (%s)",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }

    MSG("setting sample rate to %i", track.sample_rate);	
    if ((err = snd_pcm_hw_params_set_rate_near (id->pcm, id->hw_params, &(track.sample_rate), 0)) < 0) {
	MSG("Cannot set sample rate (%s)",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }

    MSG("setting channel count to %i", track.num_channels);
    if ((err = snd_pcm_hw_params_set_channels (id->pcm, id->hw_params, track.num_channels)) < 0) {
	MSG("cannot set channel count (%s)",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }

    MSG("setting hardware parameters");	
    if ((err = snd_pcm_hw_params (id->pcm, id->hw_params)) < 0) {
	MSG("cannot set parameters (%s) %d",
		 snd_strerror (err), snd_pcm_state(id->pcm));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }


    MSG("preparing");
    if ((err = snd_pcm_prepare (id->pcm)) < 0) {
	MSG("cannot prepare audio interface for use (%s)",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }

    /* Get period size. */
    snd_pcm_uframes_t period_size;
    snd_pcm_hw_params_get_period_size(id->hw_params, &period_size, 0);
    /* Calculate size of silence at end of buffer. */
    size_t samples_per_period = period_size * track.num_channels;
    MSG("samples per period = %i", samples_per_period);
    MSG("num_samples = %i", track.num_samples);
    size_t silent_samples = samples_per_period - (track.num_samples % samples_per_period);
    MSG("silent samples = %i", silent_samples);
    /* Calculate space needed to round up to nearest period size. */
    size_t volume_size = bytes_per_sample*(track.num_samples + silent_samples);
    MSG("volume size = %i", volume_size);

    /* Create a copy of track with adjusted volume. */
    MSG("making copy of track and adjusting volume");
    track_volume = track;
    track_volume.samples = (short*) malloc(volume_size);
    real_volume = ((float) id->volume + 100)/(float)200;
    for (i=0; i<=track.num_samples-1; i++)
        track_volume.samples[i] = track.samples[i] * real_volume;

    if (silent_samples > 0) {
        /* Fill remaining space with silence */
        MSG("filling with silence");
        /* TODO: This hangs.  Why?
        snd_pcm_format_set_silence(format,
            track_volume.samples + (track.num_samples * bytes_per_sample), silent_samples);
        */
        u_int16_t silent16;
        u_int8_t silent8;
        switch (bytes_per_sample) {
            case 2:
                silent16 = snd_pcm_format_silence_16(format);
                for (i = 0; i < silent_samples; i++)
                    track_volume.samples[track.num_samples + i] = silent16;
                break;
            case 1:
		/*  */
		silent8 = snd_pcm_format_silence(format);
		for (i = 0; i < silent_samples; i++)
				    track_volume.samples[track.num_samples + i] = silent8;
		                break;
        }
    }

    pthread_mutex_unlock(&id->pcm_mutex);

    /* Loop until all samples are played on the device. */
    output_samples = track_volume.samples;
    num_bytes = (track.num_samples + silent_samples)*bytes_per_sample;
    MSG("ALSA: %i bytes to output\n", num_bytes);
    while(num_bytes > 0) {

	pthread_mutex_lock(&id->pcm_mutex);
        /* See if alsa_stop was called, in which case state will change to SETUP. */
        state = snd_pcm_state(id->pcm);
        MSG("pcm state: %s", snd_pcm_state_name(state));
	if ((state != SND_PCM_STATE_RUNNING)
	    && (state != SND_PCM_STATE_PREPARED))
	{
	    /* The device was stopped in the meantime */
	    MSG("Device stopped in meantine");
            free(track_volume.samples);
	    pthread_mutex_unlock(&id->pcm_mutex);
	    return 0;
	}

	/* Write as much samples as possible */
        framecount = num_bytes/bytes_per_sample/track.num_channels;
        if (framecount < period_size) framecount = period_size;

        /* Hynek: Try with and without the following statement. */
        /*if (framecount > 2000) framecount = 2000;*/

	pthread_mutex_unlock(&id->pcm_mutex);
        // MSG("ALSA: Writing %i frames\n", framecount);
        /* This will block until either ALSA is ready for more input or
           snd_pcm_drop() has been called by another thread. */
	/* TODO: remove, debug message */
	MSG("write");
	ret = snd_pcm_writei (id->pcm, output_samples, framecount);
	/* WARNING: Looks like we never get here after stop is called when snd_pcm_writei
	 is blocking! */
        MSG("Wrote %i frames.", ret);


        if (ret == -EAGAIN || (ret >= 0 && ret < framecount)) {
            /* MSG("ALSA: Waiting for ALSA pcm to come ready\n"); */
            snd_pcm_wait(id->pcm, 1000);
        } else if (ret == -EPIPE) {
            if (xrun(id) != 0) goto error_exit;
        } else if (ret == -ESTRPIPE) {
            if (suspend(id) != 0) goto error_exit;
        } else if (ret == -EBUSY){
	    /* TODO: This should use something more sophisticated, the sleep
	       should be interruptible and prevent buffer underruns as in
	       oss.c. But maybe the asynchronous mode or some other Alsa
	       function could be used for that purpose. */
            /* In blocking mode, we don't come here. */
            /* MSG("ALSA: sleeping"); */
            usleep(100);
            continue;
        } else if (ret < 0) {

	    /* Some error happened or snd_pcm_drop() was called */
	    MSG("Write to audio interface failed (%s)",
		     snd_strerror (ret));
	    goto error_exit;
	}
        if (ret > 0) {

            /* Update counter of bytes left and move the data pointer */
            num_bytes -= ret*bytes_per_sample*track.num_channels;
            output_samples += ret*bytes_per_sample*track.num_channels/2;
        }
    }

    /* This should wait until the whole wave is played */
    /* NOTE: I'm not sure if this is interruptible by alsa_stop() */
    snd_pcm_drain(id->pcm);

    free(track_volume.samples);

    MSG("alsa_play normal exit");

    return 0;

error_exit:
    free(track_volume.samples);
    MSG("alsa_play abnormal exit");
    return -1;
}

/*
 Stop the playback on the device and interrupt alsa_play()
*/
int
alsa_stop(AudioID *id)
{
    int ret;

    /* TODO: remove, debug message */
    MSG("alsa_stop() called%s", snd_pcm_state_name(snd_pcm_state(id->pcm)));
    
    if (id == NULL) return 0;

    pthread_mutex_lock(&id->pcm_mutex);

    //    if (snd_pcm_state(id->pcm) != SND_PCM_STATE_RUNNING) return 0;
    MSG("drop() (before: %s)", snd_pcm_state_name(snd_pcm_state(id->pcm)));
    /* Stop the playback, this will interrupt alsa_play() */
    snd_pcm_drop(id->pcm);
    MSG("drop() (after: %s)", snd_pcm_state_name(snd_pcm_state(id->pcm)));
    //    snd_pcm_drain(id->pcm);
    //    MSG("drain() (after) %s", snd_pcm_state_name(snd_pcm_state(id->pcm)));
    
    pthread_mutex_unlock(&id->pcm_mutex);

    return 0;
}

/*
 Close the device
*/
int
alsa_close(AudioID *id)
{
    int err;

    /* Close device */
    if ((err = _alsa_close(id)) < 0) { 
	MSG("Cannot close audio device (%s)");
	return -1;
    }
    MSG("ALSA closed.");

    /* Destroy mutexes, free memory */
    pthread_mutex_destroy(&id->pcm_mutex);
    snd_pcm_hw_params_free (id->hw_params);

    id = NULL;

    return 0;
}

/* 
  Set volume

  Comments: I don't know how to set individual track volume with Alsa, so we
   handle volume in alsa_play() by scalar multiplication of each sample.
*/
int
alsa_set_volume(AudioID*id, int volume)
{
    return 0;
}

/* Provide the Alsa backend. */
AudioFunctions alsa_functions = {alsa_open, alsa_play, alsa_stop, alsa_close, alsa_set_volume};
