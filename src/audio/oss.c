
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
 * $Id: oss.c,v 1.1 2004-11-14 11:52:11 hanke Exp $
 */

/* Open OSS device
   Arguments:
     **pars:
      (char*) pars[0] -- the name of the device (e.g. "/dev/dsp")
      (void*) pars[1] = NULL 
*/
int
oss_open(AudioID *id, void **pars)
{
    char *device_name;

    if (id == NULL) return 0;

    if (pars[0] != NULL) device_name=(char*) pars[0];

    pthread_mutex_init(&id->fd_mutex, NULL);
    pthread_mutex_lock(&id->fd_mutex);

    id->fd = open(device_name, O_WRONLY | O_NONBLOCK, 0);
    if (id->fd == -1){
	perror(device_name);
	id = NULL;
	pthread_mutex_unlock(&id->fd_mutex);
	return -1;
    }

    pthread_cond_init(&id->pt_cond, NULL);
    pthread_mutex_init(&id->pt_mutex, NULL);

    pthread_mutex_unlock(&id->fd_mutex);

    return 0; 
}

/* Internal function. */
int
oss_sync(AudioID *id)
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
    int ret;
    struct timeval now;
    struct timespec timeout;
    float lenght;
    int MAX_BUF=32768;
    int r;

    int format, oformat, channels, speed;

    int num_bytes;
    signed short* output_samples;
    int delay = 0;
    int DELAY = 10000000;	/* nanoseconds */

    float real_volume;
    int i;

    if (id == NULL) return -1;

    /* Adjust the volume */
    real_volume = ((float) id->volume + 100)/(float)200;
    for (i=0; i<=track.num_samples-1; i++)
	track.samples[i] = track.samples[i] * real_volume;

    /* Choose the correct format */
    if (track.bits == 16)
	format = AFMT_S16_NE;
    else if (track.bits == 8)
       format = AFMT_S8;
    else{
	fprintf(stderr, "Audio: Unrecognized sound data format.\n");
	return -10;
    }
    oformat = format;	
    ret = ioctl(id->fd, SNDCTL_DSP_SETFMT, &format);
    if (ret == -1){
	perror("format");
	return -1;
    }
    if (format != oformat){
	fprintf(stderr, "Device doesn't support 16-bit sound format.\n");
	return -2;
    }

    /* Choose the correct number of channels*/
    channels = track.num_channels;
    ret = ioctl(id->fd, SNDCTL_DSP_CHANNELS, &channels);
    if (ret == -1){
	perror("channels");
	return -3;
    }
    if (channels != track.num_channels){
	fprintf(stderr, "Device doesn't support stereo sound.\n");
	return -4;
    }
    
    /* Choose the correct sample rate */
    speed = track.sample_rate;
    ret = ioctl(id->fd, SNDCTL_DSP_SPEED, &speed);
    if (ret == -1){
	perror("speed");
	return -5;
    }
    if (speed != track.sample_rate){
	fprintf(stderr, "Device doesn't support bitrate %d.\n", track.sample_rate);
	return -6;
    }

    /* Is it not an empty track? */
    if (track.samples == NULL) return 0;

    /* Loop until all samples are played on the device.
       In the meantime, wait in pthread_cond_timedwait for more data
       or for interruption. */
    output_samples = track.samples;
    num_bytes = track.num_samples*2;
    while(num_bytes > 0) {
	/* A non-blocking write */
        ret = write(id->fd, output_samples, num_bytes > MAX_BUF ? MAX_BUF : num_bytes);

	/* Send SYNC before the last write() */
	if (MAX_BUF > num_bytes) oss_sync(id);
	    
	/* Handle write() errors */
	if (ret <= 0){
	    if (errno == EAGAIN){
		continue;
	    }
	    perror("audio");
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
        gettimeofday(&now);
        timeout.tv_sec = now.tv_sec + (int) lenght;
        timeout.tv_nsec = now.tv_usec * 1000 + (lenght - (int) lenght) * 1000000000;
	if ((timeout.tv_nsec>DELAY) & !delay) timeout.tv_nsec -= (delay=DELAY);
        r = pthread_cond_timedwait(&id->pt_cond, &id->pt_mutex, &timeout);
	pthread_mutex_unlock(&id->pt_mutex);

	/* The pthread_cond_timedwait was interrupted by change in the
	 condition variable? if so, terminate.*/
        if (r != ETIMEDOUT) break;               

	num_bytes -= ret;
	output_samples += ret/2;
    }

    /* ...one more excersise in timing magic. 
       Wait for the resting DELAY nsecs. */
    if ((delay > 0) && (r == ETIMEDOUT)){
	pthread_mutex_lock(&id->pt_mutex);
	gettimeofday(&now);
	timeout.tv_sec = 0;
	timeout.tv_nsec = delay;
	r = pthread_cond_timedwait(&id->pt_cond, &id->pt_mutex, &timeout);
	pthread_mutex_unlock(&id->pt_mutex);
    }

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
    if (id == NULL) return 0;

    pthread_mutex_lock(&id->fd_mutex);
    close(id->fd);
    id->fd = 0;
    pthread_mutex_unlock(&id->fd_mutex);

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
