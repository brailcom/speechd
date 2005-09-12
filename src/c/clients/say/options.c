 /*
 * options.c - Parse and process possible command line options
 *
 * Copyright (C) 2003 Brailcom, o.p.s.
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
 * $Id: options.c,v 1.4 2005-09-12 14:28:12 hanke Exp $
 */

/* NOTE: Be careful not to include options.h, we would
   get repetitive initializations warnings */

#define PACKAGE "spd-say"
#define VERSION "0.3"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "options.h"

void
options_print_help(char *argv[])
{
    assert(argv);
    assert(argv[0]);

    printf("Usage: %s [options] \"some text\"\n", argv[0]);
    printf("Speech Dispatcher Say -- a simple client for speech synthesis (GNU GPL)\n\n");
    printf(
	   "-r, --rate             -     Set the rate of the speech\n"
           "                               (between -100 and +100, 0 is default)\n"
	   "-p, --pitch            -     Set the pitch of the speech\n"
	   "                               (between -100 and +100, 0 is default)\n"
	   "-i, --volume           -     Set the volume (intensity) of the speech\n"
           "                               (between -100 and +100, 0 is default)\n"
	   "-o, --output-module    -     Set the output module\n"
	   "-l, --language         -     Set the language (iso code)\n"
	   "-t, --voice-type       -     Set the prefered voice type\n"
           "                               (male1, male2, male3, female1, female2\n"
	   "                                female3, child_male, child_female)\n"
	   "-m, --punctuation-mode -     Set the punctuation mode (none, some, all) \n"
           "-P, --priority         -     Set priority of the message (important, message,\n"
           "                                text, notification, progress)\n"
	   "-s, --spelling         -     Spell the message\n"
           "-w, --wait             -     Wait till the message is spoken or discarded\n"
	   "-v, --version          -     Print version and copyright info\n"
	   "-h, --help             -     Print this info\n"
           "\n"
	   "Copyright (C) 2003 Brailcom, o.p.s.\n"
	   "This is free software; you can redistribute it and/or modify it\n"
	   "under the terms of the GNU General Public License as published by\n"
	   "the Free Software Foundation; either version 2, or (at your option)\n"
	   "any later version. Please see COPYING for more details.\n\n"
	   "Please report bugs to <speechd-bugs@freebsoft.org>\n\n"
	   );
    
}

void
options_print_version()
{
    printf(PACKAGE" "VERSION"\n");
    printf("Copyright (C) 2002-2005 Brailcom, o.p.s.\n"
           "spd-say comes with ABSOLUTELY NO WARRANTY.\n"
           "You may redistribute copies of spd-say\n"
           "under the terms of the GNU General Public License.\n"
           "For more information about these matters, see the file named COPYING.\n"
           );
}

#define OPT_SET_INT(param) \
    val = strtol(optarg, &tail_ptr, 10); \
    if(tail_ptr != optarg){ \
        param = val; \
    }else{ \
        printf("Syntax error or bad parameter!\n"); \
        options_print_help(argv); \
        exit(1); \
    }

#define OPT_SET_STR(param) \
    if(optarg != NULL){ \
        param = (char*) strdup(optarg); \
    }else{ \
        printf("Missing argument!\n"); \
        options_print_help(argv); \
        exit(1); \
    }

void
options_parse(int argc, char *argv[])
{
    char* tail_ptr;
    int c_opt;
    int option_index;
    int val;

    assert (argc>0);
    assert(argv);

    while(1){
        option_index = 0;
    
        c_opt = getopt_long(argc, argv, short_options, long_options,
                            &option_index);
        if (c_opt == -1) break;
        switch(c_opt){
        case 'r':
            OPT_SET_INT(rate);
            break;
        case 'p':
            OPT_SET_INT(pitch);
            break;
        case 'i':
            OPT_SET_INT(volume);
            break;
        case 'o':
            OPT_SET_STR(output_module);
            break;
        case 'l':
            OPT_SET_STR(language);
            break;
        case 't':
            OPT_SET_STR(voice_type);
            break;
        case 'm':
            OPT_SET_STR(punctuation_mode);
            break;
	case 's':
	    spelling = 1;
	    break;
	case 'P':
	    OPT_SET_STR(priority);
	    break;
	case 'w':
	    wait_till_end = 1;
	    break;
        case 'v':
            options_print_version(argv);
            exit(0);
            break;
        case 'h':
            options_print_help(argv);
            exit(0);
            break;
        default:
            printf("Unrecognized option\n");
            options_print_help(argv);
            exit(1);
        }
    }
}
#undef SPD_OPTION_SET_INT
