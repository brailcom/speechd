
 /*
  * index_marking.c -- Implements functions handling index marking
  *                    for Speech Dispatcher
  * 
  * Copyright (C) 2001,2002,2003 Brailcom, o.p.s
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
  * $Id: index_marking.c,v 1.1 2003-09-20 17:30:53 hanke Exp $
  */

#include "index_marking.h"

/* Insert index marks to the given message _msg_. Index marks have
the form @number@, the numbers begin with 0. It also escape `@' with
`@@'.*/
void
insert_index_marks(TSpeechDMessage *msg)
{
    GString *marked_text;
    int i;
    size_t len;
    char* pos;
    char character[6];
    char character2[6];
    gunichar u_char;
    int n = 0;
    int ret;
    int insert = 0;

    marked_text = g_string_new("");

    MSG(5, "MSG before index marking: |%s|", msg->buf);
       
    pos = msg->buf;
    while(pos){
        ret = spd_utf8_read_char(pos, character);
        if (ret == 0 || (strlen(character) == 0)) break;
        u_char = g_utf8_get_char(character);

        if (u_char == '@'){
            g_string_append_printf(marked_text, "@@");
        }
        else if (u_char == '.'){
            pos = g_utf8_find_next_char(pos, NULL);   
            ret = spd_utf8_read_char(pos, character2);
            if ((ret == 0) || (strlen(character2) == 0)) break;            
            u_char = g_utf8_get_char(character2);
            if (g_unichar_isspace(u_char)){
                g_string_append_printf(marked_text, "%s@%d@%s", character, n, character2);
                n++;
            }else{
                g_string_append_printf(marked_text, "%s%s", character, character2);
            }
        }
        else{
            g_string_append_printf(marked_text, "%s", character);
        }
        
        pos = g_utf8_find_next_char(pos, NULL);   
    }

    spd_free(msg->buf);
    msg->buf = marked_text->str;
    
    g_string_free(marked_text, 0);

    MSG(5, "MSG after index marking: |%s|", msg->buf);
}

/* Finds the next index mark, starting from the pointer
_buf_. If the index mark is encountered, it's number is 
returned in _mark_, the position after it's end is returned
as the return value of this function and the position
of it's beginning is returned in _*begin_*, if _begin_
is not NULL. It returns NULL if it encounters the end
of the string _buf_. */
char*
find_next_index_mark(char *buf, int *mark, char **begin)
{
    char *pos;
    char character[6];
    gunichar u_char;
    int index_mark = 0;
    GString *num;
    int ret;

    if (buf == NULL || mark == NULL) return NULL;

    pos = buf;    
    num = g_string_new("");
    while(pos){
        ret = spd_utf8_read_char(pos, character);
        if (ret == 0 || strlen(character) == 0) return NULL;

        u_char = g_utf8_get_char(character);
        if (index_mark == 2){
            if (u_char != '@') index_mark = 1;
            else if (u_char == '@'){
                index_mark = 0;
                pos = g_utf8_find_next_char(pos, NULL);
                continue;
            }
        }
        if (index_mark == 1) g_string_append(num, character);
        if (u_char == '@'){
            if (index_mark == 0){
                if (begin != NULL) *begin = pos;
                index_mark = 2;
            }
            else if (index_mark == 1){
                char *tailptr;
                *mark = strtol(num->str, &tailptr, 0);
                if (tailptr == num->str){
                    MSG(4, "Invalid index mark -- Not a number! (%s)\n", num->str);
                    return NULL;
                }
                g_string_free(num, 1);
                pos = g_utf8_find_next_char(pos, NULL);
                if (pos == NULL) return NULL;
                MSG(5, "returning position of index %d |%s|", *mark, pos);
                return pos;
            }
        }
        pos = g_utf8_find_next_char(pos, NULL);
    }        
}

/* Finds the index mark specified in _mark_ by iterating
with find_next_index_mark() through _msg->buf_. Returns
the position after it's end. */
char*
find_index_mark(TSpeechDMessage *msg, int mark)
{
    int i;
    char *pos;
    int im;

    pos = msg->buf; 
    MSG(5, "Trying to find index mark %d in |%s|", mark, msg->buf);
    while(pos = find_next_index_mark(pos, &im, NULL)){
        if (im == mark) return pos;
    }

    MSG(5, "Index mark not found.");
    return NULL;
}

/* Deletes all index marks from the given text and substitutes
`@@' for `@'*/
char*
strip_index_marks(char *buf)
{
    char *pos;
    GString *str;
    char *strret;
    char character[6];
    char character2[6];
    int inside_mark = 0;
    int ret;
    gunichar u_char;

    str = g_string_new("");
    pos = buf;

    MSG(5, "Message before stripping index marks: |%s|", buf);

    while(pos){
        ret = spd_utf8_read_char(pos, character);
        if (ret == 0 || (strlen(character) == 0)) break;
        u_char = g_utf8_get_char(character);

        if (u_char == '@'){          
            if (inside_mark){
                inside_mark = 0;
            }else{                
                pos = g_utf8_find_next_char(pos, NULL);   
                ret = spd_utf8_read_char(pos, character2);
                if ((ret == 0) || (strlen(character2) == 0)) break;            
                
                u_char = g_utf8_get_char(character2);
                if (u_char == '@'){
                    g_string_append_printf(str, "@");
                }else{
                    inside_mark = 1;
                }
            }
        }else{
          if (!inside_mark) g_string_append_printf(str, "%s", character);
        }
        
        pos = g_utf8_find_next_char(pos, NULL);   
    }

    strret = str->str;
    g_string_free(str, 0);

    MSG(5, "Message after stripping index marks: |%s|", strret);
 
   return strret;
}
