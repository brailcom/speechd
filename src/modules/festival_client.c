/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                        Copyright (c) 1999                             */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author :  Alan W Black (awb@cstr.ed.ac.uk)                */
/*             Date   :  March 1999                                      */
/*             Modified: Hynek Hanke (hanke@volny.cz) (2003)             */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Client end of Festival server API in C designed specifically formerly */
/* for Galaxy Communicator use, but rewritten to suit Speech Dispatcher  */
/* needs. Please look also at the original festival_client.c library     */
/* that can be found in festival/examples/festival_client.c -- it will   */
/* be probably more up-to-date.                                          */
/*                                                                       */
/* This can be also compiled as a stand-alone program for testing        */
/* purposes:                                                             */
/*    gcc -o festival_client -DSTANDALONE festival_client.c              */
/* and run as:                                                           */
/*    ./festival_client                                                  */
/* This creates a file test.snd, containing the synthesized waveform     */
/* You can run for example:                                              */
/*    play test.snd                                                      */
/* to hear the message                                                   */
/*=======================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

/* I'm including my local .h, not the Alan's one! */
#include "festival_client.h"

/* For testing endianness */
int fapi_endian_loc = 1;

static char *socket_receive_file_to_buff(int fd,int *size);

void delete_FT_Wave(FT_Wave *wave)
{
    if (wave != 0)
        {
            if (wave->samples != 0)
                free(wave->samples);
            free(wave);
        }
}

int save_FT_Wave_snd(FT_Wave *wave, const char *filename)
{
    FILE *fd;
    struct {
	unsigned int    magic;	/* magic number */
	unsigned int    hdr_size;	/* size of this header */
	int    data_size;	        /* length of data (optional) */
	unsigned int    encoding;	/* data encoding format */
	unsigned int    sample_rate; /* samples per second */
	unsigned int    channels;	 /* number of interleaved channels */
    } header;
    short sw_short;
    int i;

    if ((filename == 0) ||
	(strcmp(filename,"stdout") == 0) ||
	(strcmp(filename,"-") == 0))
	fd = stdout;
    else if ((fd = fopen(filename,"wb")) == NULL)
        {
            fprintf(stderr,"save_FT_Wave: can't open file \"%s\" for writing\n",
                    filename);
            return -1;
        }
    
    header.magic = (unsigned int)0x2e736e64;
    header.hdr_size = sizeof(header);
    header.data_size = 2 * wave->num_samples;
    header.encoding = 3; /* short */
    header.sample_rate = wave->sample_rate;
    header.channels = 1;
    if (FAPI_LITTLE_ENDIAN)
        {   /* snd is always sparc/68000 byte order */
            header.magic = SWAPINT(header.magic);
            header.hdr_size = SWAPINT(header.hdr_size);
            header.data_size = SWAPINT(header.data_size);
            header.encoding = SWAPINT(header.encoding);
            header.sample_rate = SWAPINT(header.sample_rate);
            header.channels = SWAPINT(header.channels);
        }
    /* write header */
    if (fwrite(&header, sizeof(header), 1, fd) != 1)
	return -1;
    if (FAPI_BIG_ENDIAN)
	fwrite(wave->samples,sizeof(short),wave->num_samples,fd);
    else
        {  /* have to swap */
            for (i=0; i < wave->num_samples; i++)
                {
                    sw_short = SWAPSHORT(wave->samples[i]);
                    fwrite(&sw_short,sizeof(short),1,fd);
                }
        }

    if (fd != stdout)
	fclose(fd);
    return 0;
}

void delete_FT_Info(FT_Info *info)
{
    if (info != 0)
	free(info);
}

static int festival_socket_open(const char *host, int port)
{   
    /* Return an FD to a remote server */
    struct sockaddr_in serv_addr;
    struct hostent *serverhost;
    int fd;
    int ret;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)  
        {
            fprintf(stderr,"festival_client: can't get socket\n");
            return -1;
        }
    memset(&serv_addr, 0, sizeof(serv_addr));
    if ((serv_addr.sin_addr.s_addr = inet_addr(host)) == -1)
        {
            /* its a name rather than an ipnum */
            serverhost = gethostbyname(host);
            if (serverhost == (struct hostent *)0)
                {
                    fprintf(stderr,"festival_client: gethostbyname failed\n");
                    return -1;
                }
            memmove(&serv_addr.sin_addr,serverhost->h_addr, serverhost->h_length);
        }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
        {
            fprintf(stderr,"festival_client: connect to server failed\n");
            return -1;
        }


    return fd;
}

static int nist_get_param_int(char *hdr, char *field, int def_val)
{
    char *p;
    int val;

    if (((p=strstr(hdr,field)) != NULL) &&
	(strncmp(" -i ",p+strlen(field),4) == 0))
        {
            sscanf(p+strlen(field)+4,"%d",&val);
            return val;
        }
    else
	return def_val;
}

static int nist_require_swap(char *hdr)
{
    char *p;
    char *field = "sample_byte_format";

    if ((p=strstr(hdr,field)) != NULL)
        {
            if (((strncmp(" -s2 01",p+strlen(field),7) == 0) && FAPI_BIG_ENDIAN) ||
                ((strncmp(" -s2 10",p+strlen(field),7) == 0) 
                 && FAPI_LITTLE_ENDIAN))
                return 1;
        }
    return 0; /* if unknown assume native byte order */
}

static char *client_accept_s_expr(int fd)
{
    /* Read s-expression from server, as a char * */
    char *expr;
    int filesize;

    expr = socket_receive_file_to_buff(fd,&filesize);
    expr[filesize] = '\0';
    return expr;
}

static FT_Wave *client_accept_waveform(int fd)
{
    /* Read waveform from server */
    char *wavefile;
    int filesize;
    int num_samples, sample_rate, i;
    FT_Wave *wave;

    wavefile = socket_receive_file_to_buff(fd,&filesize);
    wave = NULL;
    
    /* I know this is NIST file and its an error if it isn't */
    if (filesize >= 1024)
        {
            /* If this doesn't work, probably you forgot to set
             the output file type to NIST ! by Parameter.set */
            num_samples = nist_get_param_int(wavefile,"sample_count",1);
            sample_rate = nist_get_param_int(wavefile,"sample_rate",16000);
            
            if ((num_samples*sizeof(short))+1024 == filesize)
                {		
                    wave = (FT_Wave *)malloc(sizeof(FT_Wave));
                    wave->num_samples = num_samples;
                    wave->sample_rate = sample_rate;
                    wave->samples = (short *) malloc(num_samples*sizeof(short));
                    memmove(wave->samples, wavefile+1024, num_samples*sizeof(short));
                    if (nist_require_swap(wavefile))
                        for (i=0; i < num_samples; i++)
                            wave->samples[i] = SWAPSHORT(wave->samples[i]);
                }
        }
    
    if (wavefile != 0)  /* just in case we've got an ancient free() */
	free(wavefile);

    return wave;
}

static char *socket_receive_file_to_buff(int fd,int *size)
{
    /* Receive file (probably a waveform file) from socket using   */
    /* Festival key stuff technique, but long winded I know, sorry */
    /* but will receive any file without closing the stream or     */
    /* using OOB data                                              */
    static char *file_stuff_key = "ft_StUfF_key"; /* must == Festival's key */
    char *buff;
    int bufflen;
    int n,k,i;
    char c;

    bufflen = 1024;
    buff = (char *)malloc(bufflen);
    *size=0;

    for (k=0; file_stuff_key[k] != '\0';)
    {
	n = read(fd,&c,1);
	if (n==0) break;  /* hit stream eof before end of file */
	if ((*size)+k+1 >= bufflen)
	{   /* +1 so you can add a NULL if you want */
	    bufflen += bufflen/4;
	    buff = (char *)realloc(buff,bufflen);
	}
	if (file_stuff_key[k] == c)
	    k++;
	else if ((c == 'X') && (file_stuff_key[k+1] == '\0'))
	{   /* It looked like the key but wasn't */
	    for (i=0; i < k; i++,(*size)++) 
		buff[*size] = file_stuff_key[i];
	    k=0;
	    /* omit the stuffed 'X' */
	}
	else
	{
	    for (i=0; i < k; i++,(*size)++)
		buff[*size] = file_stuff_key[i];
	    k=0;
	    buff[*size] = c;
	    (*size)++;
	}

    }

    return buff;
}

int
festival_get_ack(FT_Info **info, char* ack)
{
    int read_bytes;
    int n;
    for (n=0; n < 3; ){
        read_bytes = read((*info)->server_fd, ack+n, 3-n);
        if (read_bytes == 0){
            /* WARNING: This is a very strange situation
               but it happens often, I don't really know
               why???*/
            fprintf(stderr, "FESTIVAL CLOSED CONNECTION, REOPENING");
            *info = festivalOpen(*info);
            festival_connection_crashed = 1;
            return 1; 
        }
        n += read_bytes;                
    }
    ack[3] = '\0';
    return 0;
}

void
festival_accept_any_response(FT_Info *info)
{
    char ack[4];
    do {
        if(festival_get_ack(&info, ack)) return;

	if (strcmp(ack,"WV\n") == 0){         /* receive a waveform */
	    client_accept_waveform(info->server_fd);
	}else if (strcmp(ack,"LP\n") == 0)    /* receive an s-expr */
	    client_accept_s_expr(info->server_fd);
	else if (strcmp(ack,"ER\n") == 0)    /* server got an error */
            {
                /* This message ER is returned even if it was because there was
                   no sound produced, for this reason, the warning is disabled */
                /* fprintf(stderr,"festival_client: server returned error\n"); */
                break;
            }
    }while (strcmp(ack,"OK\n") != 0);
}

/***********************************************************************/
/* Public Functions to this API                                        */
/***********************************************************************/

/* Opens a connection to Festival server (which must be running)
 * and returns it's identification in new FT_Info */
FT_Info *
festivalOpen(FT_Info *info)
{
    FILE *fd;

    /* Open socket to server */

    festival_connection_crashed = 0;

    if (info == 0)
	info = festivalDefaultInfo();

    info->server_fd = 
	festival_socket_open(info->server_host, info->server_port);

    if (info->server_fd == -1){
        delete_FT_Info(info);
	return NULL;
    }

    /* Opens a stream associated to the socket */
    fd = fdopen(dup(info->server_fd),"wb");
    /* Send the command and data */
    fprintf(fd,
            "(define (speechd_tts_textall string mode)\n"
            "  (let ((tmpfile (make_tmp_filename))\n"
            "        (fd))\n"
            "    (set! fd (fopen tmpfile \"wb\"))\n"
            "    (format fd \"%%s\" string)\n"
            "    (fclose fd)\n"
            "    (set! tts_hooks (list utt.synth save_record_wave))\n"
            "    (set! wavefiles nil)\n"
            "    (tts_file tmpfile mode)\n"
            "    (delete-file tmpfile)\n"
            "    (let ((utt (combine_waves)))\n"
            "      (if (member 'Wave (utt.relationnames utt))\n"
            "          (utt.send.wave.client utt)))))\n"
            );
    fclose(fd);

    festival_accept_any_response(info);

    fd = fdopen(dup(info->server_fd),"wb");
    /* Send the command and data */
    fprintf(fd,"(Parameter.set 'Wavefiletype 'nist)\n");
    fclose(fd);

    festival_accept_any_response(info);

    return info;
}

/* Sends a TEXT to Festival server for synthesis. Doesn't
 * wait for reply (see festivalStringToWaveGetData()). 
 * Please make sure that the socket is empty before calling
 * this command -- otherwise, you would get another message
 * in response.
 * Returns 0 if everything is ok, -1 otherwise.
*/
int
festivalStringToWaveRequest(FT_Info *info, char *text, int spelling)
{
    FT_Wave *wave;
    FILE *fd;
    char *p;
    char ack[4];
    int n;

    if (info == 0)
	return -1;
    if (info->server_fd == -1)
    {
	fprintf(stderr,"festival_client: server connection unopened\n");
	return -1;
    }

    /* Opens a stream associated to the socket */
    fd = fdopen(dup(info->server_fd),"wb");

    /* Send the command and data */
    fprintf(fd,"(speechd_tts_textall \"");
    /* Copy text over to server, escaping any quotes */
    for (p=text; p && (*p != '\0'); p++)
    {
	if ((*p == '"') || (*p == '\\'))
	    putc('\\',fd);
	putc(*p, fd);
    }

    /* Specify mode */
    if (!spelling) fprintf(fd,"\" nil)\n");
    else fprintf(fd,"\" 'spell)\n");

    /* Close the stream (but not the socket) */
    fclose(fd);
    return 0;
}

/* Reads the wavefile sent back after festivalStringToWaveRequest()
 * has been called. This function blocks until all the data is
 * available. Note that for longer messages this can be quite long
 * on some slower machines. */
FT_Wave*
festivalStringToWaveGetData(FT_Info *info)
{
    FT_Wave *wave = NULL;
    int n;
    char ack[5];
    int read_bytes;

    /* Read back info from server */
    /* This assumes only one waveform will come back, also LP is unlikely */
    wave = NULL;
    do {
        if (festival_get_ack(&info, ack)) return NULL;
	if (strcmp(ack,"WV\n") == 0){         /* receive a waveform */
            //            fprintf(stderr,"point 2");
	    wave = client_accept_waveform(info->server_fd);
	}else if (strcmp(ack,"LP\n") == 0){    /* receive an s-expr */
            //            fprintf(stderr,"point 3");
	    client_accept_s_expr(info->server_fd);
	}else if (strcmp(ack,"ER\n") == 0)    /* server got an error */
            {
                //                fprintf(stderr,"festival_client: server returned error\n");
                break;
            }
    } while (strcmp(ack,"OK\n") != 0);
    
    return wave;
}

/* Closes the Festival server socket connection */
int festivalClose(FT_Info *info)
{
    if (info == 0)
	return 0;

    if (info->server_fd != -1){
        FILE *fd;
        fd = fdopen(dup(info->server_fd),"wb");
        if (fd != NULL){
            fprintf(fd,"(quit)\n");
            fclose(fd);
        }
	close(info->server_fd);
    }

    return 0;
}

int
festival_read_response(FT_Info *info)
{
    char buf[4];

    if (festival_get_ack(&info, buf)) return 1;
    buf[3] = 0;

    if (!strcmp(buf,"ER\n")){
        return 1;
    }else{
        client_accept_s_expr(info->server_fd);
    }

    if (festival_get_ack(&info, buf)) return 1;

    return 0;
}

int
festivalSetVoice(FT_Info *info, char *voice)
{
    FILE* fd;
    char buf[4];
    int n;
    int read_bytes = 0;

    fd = fdopen(dup(info->server_fd),"wb");
    /* Send the command and set new voice */
    fprintf(fd,"(%s)\n", voice);
    fclose(fd);

    return festival_read_response(info);
}

int
festivalSetRate(FT_Info *info, signed int rate)
{
    FILE* fd;
    char buf[4];
    int n;
    float stretch;
    int read_bytes = 0;

    if (info == NULL) DBG("festivalSetRate called with info = NULL\n");
    if (info->server_fd == -1) DBG("festivalSetRate: server_fd invalid\n");

    stretch = 1;
    if (rate < 0) stretch -= ((float) rate) / 50;
    if (rate > 0) stretch -= ((float) rate) / 200;

    fd = fdopen(dup(info->server_fd),"wb");
    /* Send the command and set new voice */
    fprintf(fd,"(Parameter.set \"Duration_Stretch\" %f)\n", stretch);
    fclose(fd);

    return festival_read_response(info);
}

int
festivalSetPitch(FT_Info *info, signed int pitch, unsigned int mean)
{
    FILE* fd;
    char buf[4];
    int n;
    int f0;
    int read_bytes = 0;

    if (info == NULL) DBG("festivalSetPitch called with info = NULL\n");
    if (info->server_fd == -1) DBG("festivalSetPitch: server_fd invalid\n");

    f0 = 100;
    if (pitch < 0) f0 += pitch / 2;
    if (pitch > 0) f0 += pitch;

    fd = fdopen(dup(info->server_fd),"wb");
    /* Send the command and set new voice */
    fprintf(fd,"(set! int_lr_params '((target_f0_mean %d) (target_f0_std %d) (model_f0_mean 170) (model_f0_std 34)))\n", f0, mean);
    fclose(fd);

    return festival_read_response(info);
}

int
festivalSetPunctuationMode(FT_Info *info, int mode)
{
    FILE* fd;
    char buf[4];
    int n;
    int read_bytes = 0;

    if (info == NULL) DBG("festivalSetPitch called with info = NULL\n");
    if (info->server_fd == -1) DBG("festivalSetPitch: server_fd invalid\n");

    assert((mode == 0) || (mode == 1) || (mode == 2));

    fd = fdopen(dup(info->server_fd),"wb");
    /* Send the command and set new voice */
    if (mode == 0) fprintf(fd,"(set-punctuation-mode 'none)\n");
    else if (mode == 1) fprintf(fd,"(set-punctuation-mode 'all)\n");
    else if (mode == 2) fprintf(fd,"(set-punctuation-mode 'default)\n");
    fclose(fd);

    return festival_read_response(info);
}

static FT_Info *festivalDefaultInfo()
{
    FT_Info *info;
    info = (FT_Info *) malloc(sizeof(FT_Info));
    
    info->server_host = FESTIVAL_DEFAULT_SERVER_HOST;
    info->server_port = FESTIVAL_DEFAULT_SERVER_PORT;
    info->text_mode = FESTIVAL_DEFAULT_TEXT_MODE;

    info->server_fd = -1;
    
    return info;
}

/* For testing purposes, this library can be compiled as
 * a standalone program, please see the introduction at
 * the beginning  of this file
 */

#ifdef STANDALONE
int main(int argc, char **argv)
{
    char *server=0;
    int port=-1;
    char *text=0;
    char *output=0;
    char *mode=0;
    int i;
    FT_Info *info;
    FT_Wave *wave;

    printf("Welcome to Festival client library for Speech Dispatcher\n");

    info = festivalDefaultInfo();
    if (server != 0)
	info->server_host = FESTIVAL_DEFAULT_SERVER_HOST;
    if (port != -1)
	info->server_port = FESTIVAL_DEFAULT_SERVER_PORT;
    if (mode != 0)
	info->text_mode = FESTIVAL_DEFAULT_TEXT_MODE;

    info = festivalOpen(info);

    if (info == 0){
        printf("Can't open connection to festival server!\n");
        exit(1);
    }
    if (info->server_fd <= 0){
        printf("Can't open connection to festival server!\n");
        exit(1);
    }

    festivalStringToWaveRequest(info,"Hi, how are you? I'm Festival! And who are you?");
    wave = festivalStringToWaveGetData(info);

    if (wave != 0){
	save_FT_Wave_snd(wave,"test.snd");
    }else{
        printf("Wave null! Festival server doesn't work properly.\n");
        exit(1);
    }
    festivalClose(info);

    printf("The synthesized text should have been written to test.snd\n");

    return 0;
}
#endif
