
/*
 * fdsetconv.c - Conversion of types for Speech Dispatcher
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * $Id: fdsetconv.c,v 1.5 2007-06-21 20:09:45 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "fdsetconv.h"

char*
EVoice2str(EVoiceType voice)
{
    char *str;

    switch (voice)
        {
        case MALE1: str = g_strdup("male1"); break;
        case MALE2: str = g_strdup("male2"); break;
        case MALE3: str = g_strdup("male3"); break;
        case FEMALE1: str = g_strdup("female1"); break;
        case FEMALE2: str = g_strdup("female2"); break;
        case FEMALE3: str = g_strdup("female3"); break;
        case CHILD_MALE: str = g_strdup("child_male"); break;
        case CHILD_FEMALE: str = g_strdup("child_female"); break;
        default: str = NULL;
        }

    return str;
}

EVoiceType
str2EVoice(char* str)
{
    EVoiceType voice;

    if (!strcmp(str, "male1")) voice = MALE1;
    else if (!strcmp(str, "male2")) voice = MALE2;
    else if (!strcmp(str, "male3")) voice = MALE3;
    else if (!strcmp(str, "female1")) voice = FEMALE1;
    else if (!strcmp(str, "female2")) voice = FEMALE2;
    else if (!strcmp(str, "female3")) voice = FEMALE3;
    else if (!strcmp(str, "child_male")) voice = CHILD_MALE;
    else if (!strcmp(str, "child_female")) voice = CHILD_FEMALE;
    else voice = NO_VOICE;

    return voice;
}

char*
EPunctMode2str(SPDPunctuation punct)
{
    char *str;

    switch (punct)
        {
        case SPD_PUNCT_NONE: str = g_strdup("none"); break;
        case SPD_PUNCT_ALL: str = g_strdup("all"); break;
        case SPD_PUNCT_SOME: str = g_strdup("some"); break;
        default: str = NULL;
        }

    return str;
}

SPDPunctuation
str2EPunctMode(char* str)
{
    SPDPunctuation punct;

    if (!strcmp(str, "none")) punct = SPD_PUNCT_NONE;
    else if (!strcmp(str, "all")) punct = SPD_PUNCT_ALL;
    else if (!strcmp(str, "some")) punct = SPD_PUNCT_SOME;
    else punct = -1;

    return punct;
}

char*
ESpellMode2str(ESpellMode spell)
{
    char *str;

    switch (spell)
        {
        case SPELLING_ON: str = g_strdup("on"); break;
        case SPELLING_OFF: str = g_strdup("off"); break;
        default: str = NULL;
        }

    return str;
}

ESpellMode
str2ESpellMode(char* str)
{
    ESpellMode spell;

    if (!strcmp(str, "on")) spell = SPELLING_ON;
    else if (!strcmp(str, "off")) spell = SPELLING_OFF;
    else spell = -1;

    return spell;
}

char*
ECapLetRecogn2str(ECapLetRecogn recogn)
{
    char *str;

    switch (recogn)
        {
        case RECOGN_NONE: str = g_strdup("none"); break;
        case RECOGN_SPELL: str = g_strdup("spell"); break;
        case RECOGN_ICON: str = g_strdup("icon"); break;
        default: str = NULL;
        }

    return str;
}

ECapLetRecogn
str2ECapLetRecogn(char* str)
{
    ECapLetRecogn recogn;

    if (!strcmp(str, "none")) recogn = RECOGN_NONE;
    else if (!strcmp(str, "spell")) recogn = RECOGN_SPELL;
    else if (!strcmp(str, "icon")) recogn = RECOGN_ICON;
    else recogn = -1;

    return recogn;
}


EVoiceType
str2intpriority(char* str)
{
    int priority;

    if (!strcmp(str, "important"))  priority = 1;
    else if (!strcmp(str, "text")) priority = 2;
    else if (!strcmp(str, "message")) priority = 3;
    else if (!strcmp(str, "notification")) priority = 4;
    else if (!strcmp(str, "progress")) priority = 5;
    else priority = -1;

    return priority;
}
