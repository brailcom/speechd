
/*
 * oss.c -- The Open Sound System backend for the spd_audio library.
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
 * $Id: oss.c,v 1.4 2004-11-29 14:26:18 hanke Exp $
 */

int
_oss_open(AudioID *id)
{
    pthread_mutex_lock(&id->fd_mutex);

    id->fd = open(id->device_name, O_WRONLY, 0);
    if (id->fd == -1){
	perror(id->device_name);
	id = NULL;
	pthread_mutex_unlock(&id->fd_mutex);
	return -1;
    }

    pthread_mutex_unlock(&id->fd_mutex);

    return 0;
}

int
_oss_close(AudioID *id)
{
    if (id == NULL) return 0;
    if (id->fd == 0) return 0;

    pthread_mutex_lock(&id->fd_mutex);
    close(id->fd);
    id->fd = 0;
    pthread_mutex_unlock(&id->fd_mutex);
}

/* Open OSS device
   Arguments:
     **pars:
      (char*) pars[0] -- the name of the device (e.g. "/dev/dsp")
      (void*) pars[1] = NULL 
*/
int
oss_open(AudioID *id, void **pars)
{
    int ret;

    if (id == NULL) return 0;
    if (pars[0] == NULL) return -1;

    if (pars[0] != NULL) id->device_name = (char*) strdup((char*) pars[0]);

    pthread_mutex_init(&id->fd_mutex, NULL);

    pthread_cond_init(&id->pt_cond, NULL);
    pthread_mutex_init(&id->pt_mutex, NULL);

    /* Test if it's possible to access the device */
    ret = _oss_open(id);
    if (ret) return ret;
    ret = _oss_close(id);
    if (ret) return ret;


    return 0; 
}

/* Internal function. */
int
_oss_sync(AudioID *id)
{
    int ret;

    ret = ioctl(id->fd, SNDCTL_DSP_POST, 0);
    if (ret == -1){
        perror("reset");
        return -1;
    }

}

int
oss_play(AudioID *id, AudioTrack track)
{
    int ret, ret2;
    struct timeval now;
    struct timespec timeout;
    float lenght;
    int r;
    int format, oformat, channels, speed;
    int bytes_per_sample;
    int num_bytes;
    signed short* output_samples;
    float delay = 0;
    float DELAY = 0.1;	/* in seconds */
    audio_buf_info info;
    int bytes;
    float real_volume;
    int i;
    int re;  

    AudioTrack track_volume;

    if (id == NULL) return -1;

    /* Open the sound device. This is necessary for OSS so that the
     application doesn't prevent others from accessing /dev/dsp when
     it doesn't play anything. */
    ret = _oss_open(id);
    if (ret) return -2;

    /* Create a copy of track with the adjusted volume */
    track_volume = track;
    track_volume.samples = (short*) malloc(sizeof(short)*track.num_samples);
    real_volume = ((float) id->volume + 100)/(float)200;
    for (i=0; i<=track.num_samples-1; i++)
	track_volume.samples[i] = track.samples[i] * real_volume;

    /* Choose the correct format */
    if (track.bits == 16){
	format = AFMT_S16_NE;
	bytes_per_sample = 2;
    }else if (track.bits == 8){
	bytes_per_sample = 1;
	format = AFMT_S8;
    }else{
	fprintf(stderr, "Audio: Unrecognized sound data format.\n");
	_oss_close(id);
	return -10;
    }

    oformat = format;	
    ret = ioctl(id->fd, SNDCTL_DSP_SETFMT, &format);
    if (ret == -1){
	perror("format");
	_oss_close(id);
	return -1;
    }
    if (format != oformat){
	fprintf(stderr, "Device doesn't support 16-bit sound format.\n");
	_oss_close(id);
	return -2;
    }

    /* Choose the correct number of channels*/
    channels = track.num_channels;
    ret = ioctl(id->fd, SNDCTL_DSP_CHANNELS, &channels);
    if (ret == -1){
	perror("channels");
	_oss_close(id);
	return -3;
    }
    if (channels != track.num_channels){
	fprintf(stderr, "Device doesn't support stereo sound.\n");
	_oss_close(id);
	return -4;
    }
    
    /* Choose the correct sample rate */
    speed = track.sample_rate;
    ret = ioctl(id->fd, SNDCTL_DSP_SPEED, &speed);
    if (ret == -1){
	perror("speed");
	_oss_close(id);
	return -5;
    }
    if (speed != track.sample_rate){
	fprintf(stderr, "Device doesn't support bitrate %d.\n", track.sample_rate);
	_oss_close(id);
	return -6;
    }

    /* Is it not an empty track? */
    if (track.samples == NULL){
	_oss_close(id);
	return 0;
    }

    /* Loop until all samples are played on the device.
       In the meantime, wait in pthread_cond_timedwait for more data
       or for interruption. */
    output_samples = track_volume.samples;
    num_bytes = track.num_samples*bytes_per_sample;
    while(num_bytes > 0) {

	/* OSS doesn't support non-blocking write, so lets check how much date
	 can we write so that write() returns immediatelly */	
	re = ioctl(id->fd, SNDCTL_DSP_GETOSPACE, &info);
	if (re == -1){
	    perror("OSS GETOSPACE");
	    _oss_close(id);
	    return -5;
	}
	
	/* If there is not enough space for a single fragment, try later.
	   (This shouldn't happen, it has very bad effect on synchronization!) */
	if (info.fragments == 0){
	    usleep (100);
	    continue;
	}
	    
	bytes = info.fragments * info.fragsize;
	ret = write(id->fd, output_samples, num_bytes > bytes ? bytes : num_bytes);

	/* Handle write() errors */
	if (ret <= 0){
	    perror("audio");
	    _oss_close(id);
	    return -6;
	}

	num_bytes -= ret;
	output_samples += ret/2;

	/* If there is some more data that is less than a
	   full fragment, we need to write it immediatelly so
	   that it doesn't cause buffer underruns later. */
	if ((num_bytes > 0)
	    && (num_bytes < info.fragsize) 
	    && (bytes+num_bytes < info.bytes)){
	    ret2 = write(id->fd, output_samples, num_bytes);       
	    num_bytes -= ret2;
	    output_samples += ret2/2;
	    ret += ret2;
	}

	/* Handle write() errors */
	if (ret <= 0){
	    perror("audio");
	    _oss_close(id);
	    return -6;
	}

	/* Some timing magic... 
	   We need to wait for the time computed from the number of
	   samples written. But this wait needs to be interruptible
	   by oss_stop(). Furthermore, there need to be no buffer
	   underrruns, so we actually wait a bit (DELAY) less
	   in the first pass through the while() loop. Then our timer
	   will be DELAY nsecs backwards.
	*/
	pthread_mutex_lock(&id->pt_mutex);
        lenght = (((float) (ret)/2) / (float) track.sample_rate);
	if (!delay){
	    delay = lenght>DELAY ? DELAY : lenght;
	    lenght -= delay;
	}
        gettimeofday(&now);
        timeout.tv_sec = now.tv_sec + (int) lenght;
        timeout.tv_nsec = now.tv_usec * 1000 + (lenght - (int) lenght) * 1000000000;

        r = pthread_cond_timedwait(&id->pt_cond, &id->pt_mutex, &timeout);
	pthread_mutex_unlock(&id->pt_mutex);

	/* The pthread_cond_timedwait was interrupted by change in the
	 condition variable? if so, terminate.*/
        if (r != ETIMEDOUT) break;               
    }

    /* Flush all the buffers */
    _oss_sync(id);

    /* ...one more excersise in timing magic. 
       Wait for the resting DELAY nsecs. */
    if ((delay > 0) && (r == ETIMEDOUT)){
	pthread_mutex_lock(&id->pt_mutex);
	gettimeofday(&now);
	timeout.tv_sec = 0;
	timeout.tv_nsec = delay * 1000000000;
	r = pthread_cond_timedwait(&id->pt_cond, &id->pt_mutex, &timeout);
	pthread_mutex_unlock(&id->pt_mutex);
    }

    /* Close the device so that we don't block other apps trying to
       acces the device. */
    _oss_close(id);

    return 0;
}

/* Stop the playback on the device and interrupt oss_play */
int
oss_stop(AudioID *id)
{
    int ret;

    if (id == NULL) return 0;

    /* Stop the playback on /dev/dsp */
    pthread_mutex_lock(&id->fd_mutex);
    if (id->fd != 0)
	ret = ioctl(id->fd, SNDCTL_DSP_RESET, 0);
    pthread_mutex_unlock(&id->fd_mutex);
    if (ret == -1){
	perror("reset");
	return -1;
    }

    /* Interrupt oss_play by setting the condition variable */
    pthread_mutex_lock(&id->pt_mutex);
    pthread_cond_signal(&id->pt_cond);
    pthread_mutex_unlock(&id->pt_mutex);
    return 0;
}

/* Close the device */
int
oss_close(AudioID *id)
{

    /* Does nothing because the device is being automatically openned and
       closed in oss_play before and after playing each sample. */

    free(id->device_name);

    id = NULL;

    return 0;
}

/* Set volume

Comments:
  /dev/dsp can't set volume. We just multiply the track samples by
  a constant in oss_play (see oss_play() for more information).
*/
int
oss_set_volume(AudioID*id, int volume)
{
    return 0;
}

/* Provide the OSS backend. */
AudioFunctions oss_functions = {oss_open, oss_play, oss_stop, oss_close, oss_set_volume};
