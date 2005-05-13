
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
 * $Id: alsa.c,v 1.1 2005-05-13 10:37:26 hanke Exp $
 */

/* 
 Open the device so that it's ready for playing on the default device. Internal
 function used by the public alsa_open.

 TODO: I don't know how to put dmix into this. Maybe the device ``default'' is
 not the right one?
*/
int
_alsa_open(AudioID *id)
{
    int err;

    /* Open the device */
    if ((err = snd_pcm_open (&id->pcm, "default",
			     SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) { 
	fprintf (stderr, "ALSA: Cannot open audio device (%s)\n",
		 snd_strerror (err));
	return -1;
    }
   
    /* Allocate space for hw_params (description of the sound parameters) */
    if ((err = snd_pcm_hw_params_malloc (&id->hw_params)) < 0) {
	fprintf (stderr, "ALSA: Cannot allocate hardware parameter structure (%s)\n",
		 snd_strerror (err));
	return -1;
    }

    /* Initialize hw_params on our pcm */
    if ((err = snd_pcm_hw_params_any (id->pcm, id->hw_params)) < 0) {
	fprintf (stderr, "ALSA: Cannot initialize hardware parameter structure (%s)\n",
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
	fprintf (stderr, "ALSA: Cannot close ALSA device (%s)\n",
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
    
    ret = _alsa_open(id);
    if (ret){
	fprintf(stderr, "ALSA: Cannot initialize Alsa device\n");
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
    int format, channels;
    int bytes_per_sample;
    int num_bytes;
    int frames;
    signed short* output_samples;

    int bytes;
    float real_volume;
    int i;
    
    int err;

    AudioTrack track_volume;

    if (id == NULL) return -1;

    /* Is it not an empty track? */
    if (track.samples == NULL) return 0;   

    /* pcm_mutex: Other threads aren't allowed to change the state */
    pthread_mutex_lock(&id->pcm_mutex);
    /* Ensure we are in the right state */
    if ((snd_pcm_state(id->pcm) != SND_PCM_STATE_OPEN)
	&& (snd_pcm_state(id->pcm) != SND_PCM_STATE_SETUP))
	{
	    /* If draining is under way after alsa_stop, there is probably no
	       other option than to wait. */
	    while(snd_pcm_state(id->pcm) == SND_PCM_STATE_DRAINING) usleep(100);
	}

    /* Create a copy of track with adjusted volume.  
   TODO: Maybe Alsa can adjust volume itself and this can be done in a clean
   way? */
    track_volume = track;
    track_volume.samples = (short*) malloc(sizeof(short)*track.num_samples);
    real_volume = ((float) id->volume + 100)/(float)200;
    for (i=0; i<=track.num_samples-1; i++)
	track_volume.samples[i] = track.samples[i] * real_volume;

    /* Choose the correct format */
    if (track.bits == 16){
	/* TODO: Fix the endian issue since Alsa doesn't support native endian. */
	format = SND_PCM_FORMAT_S16_LE;
	bytes_per_sample = 2;
    }else if (track.bits == 8){
	bytes_per_sample = 1;
	format = SND_PCM_FORMAT_S8;
    }else{
	fprintf(stderr, "ALSA: Unrecognized sound data format, track.bits = %d\n", track.bits);	
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }

    /* Set access mode, bitrate, sample rate and channels */
    if ((err = snd_pcm_hw_params_set_access (id->pcm, id->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
	fprintf (stderr, "ALSA: Cannot set access type (%s)\n",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }
    
    if ((err = snd_pcm_hw_params_set_format (id->pcm, id->hw_params, format)) < 0) {
	fprintf (stderr, "ALSA: Cannot set sample format (%s)\n",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }
	
    if ((err = snd_pcm_hw_params_set_rate_near (id->pcm, id->hw_params, &(track.sample_rate), 0)) < 0) {
	fprintf (stderr, "ALSA: Cannot set sample rate (%s)\n",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }
	    
    if ((err = snd_pcm_hw_params_set_channels (id->pcm, id->hw_params, track.num_channels)) < 0) {
	fprintf (stderr, "cannot set channel count (%s)\n",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }
	
    if ((err = snd_pcm_hw_params (id->pcm, id->hw_params)) < 0) {
	fprintf (stderr, "cannot set parameters (%s) %d \n",
		 snd_strerror (err), snd_pcm_state(id->pcm));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }


    if ((err = snd_pcm_prepare (id->pcm)) < 0) {
	fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
		 snd_strerror (err));
	pthread_mutex_unlock(&id->pcm_mutex);
	return -1;
    }
    pthread_mutex_unlock(&id->pcm_mutex);

    /* Loop until all samples are played on the device.
       In the meantime sleep. */
    output_samples = track_volume.samples;
    num_bytes = track.num_samples*bytes_per_sample;
    while(num_bytes > 0) {

	pthread_mutex_lock(&id->pcm_mutex);
	if ((snd_pcm_state(id->pcm) != SND_PCM_STATE_RUNNING)
	    && (snd_pcm_state(id->pcm) != SND_PCM_STATE_PREPARED))
	{
	    /* The device was stopped in the meantime */
	    pthread_mutex_unlock(&id->pcm_mutex);
	    return 0;	    
	}


	/* Write as much samples as possible */
	ret = snd_pcm_writei (id->pcm, output_samples, num_bytes/bytes_per_sample/track.num_channels);
	pthread_mutex_unlock(&id->pcm_mutex);

        if (ret < 0){
	    if (ret = -EAGAIN) continue;
	    if (ret = -EBUSY){
	    /* TODO: This should use something more sophisticated, the sleep
	       should be interruptible and prevent buffer underruns as in
	       oss.c. But maybe the asynchronous mode or some other Alsa
	       function could be used for that purpose. */
		usleep(100);
		continue;
	    }

	    /* Some error happened */
	    fprintf (stderr, "ALSA: Write to audio interface failed (%s)\n",
		     snd_strerror (ret));
	    return -1;
	}

	/* Update counter of bytes left and move the data pointer */
	num_bytes -= ret*bytes_per_sample*track.num_channels;
	output_samples += ret*bytes_per_sample*track.num_channels/2;
    }
    
    /* This should wait until the whole wave is played */
    /* NOTE: I'm not sure if this is interruptible by alsa_stop() */
    snd_pcm_drain(id->pcm);
    
    return 0;
}

/*
 Stop the playback on the device and interrupt alsa_play()
*/
int
alsa_stop(AudioID *id)
{
    int ret;

    if (id == NULL) return 0;

    pthread_mutex_lock(&id->pcm_mutex);
    /* Stop the playback, this will alsa interrupt alsa_play()
     on the next write */
    snd_pcm_drop(id->pcm);
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
	fprintf (stderr, "ALSA: Cannot close audio device (%s)\n");
	return -1;
    }

    id = NULL;

    /* Destroy mutexes, free memory */
    pthread_mutex_destroy(&id->pcm_mutex);
    snd_pcm_hw_params_free (id->hw_params);

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
