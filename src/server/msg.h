/*
 * msg.h - Client/server messages for Speech Deamon
 *
 * Copyright (C) 2001,2002,2003 Ceska organizace pro podporu free software
 * (Czech Free Software Organization), Prague 2, Vysehradska 3/255, 128 00,
 * <freesoft@freesoft.cz>
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
 * $Id: msg.h,v 1.5 2003-03-25 22:48:24 hanke Exp $
 */


#define OK_LANGUAGE_SET				"201 OK LANGUAGE SET\r\n"
#define OK_PRIORITY_SET				"202 OK PRIORITY SET\r\n"
#define OK_RATE_SET					"203 OK RATE SET\r\n"
#define OK_PITCH_SET				"204 OK PITCH SET\r\n"
#define OK_PUNCT_MODE_SET			"205 OK PUNCTUATION SET\r\n"
#define OK_CAP_LET_RECOGN_SET		"206 OK CAP LET RECOGNITION SET\r\n"
#define OK_SPELLING_SET				"207 OK SPELLING SET\r\n"
#define OK_CLIENT_NAME_SET			"208 OK CLIENT NAME SET\r\n"
#define OK_STOPED					"210 OK STOPED\r\n"
#define OK_PAUSED					"211 OK PAUSED\r\n"
#define OK_RESUMED					"212 OK RESUMED\r\n"

#define OK_CUR_SET_FIRST			"220 OK CURSOR SET FIRST\r\n"
#define OK_CUR_SET_LAST				"221 OK CURSOR SET LAST\r\n"
#define OK_CUR_SET_POS				"222 OK CURSOR SET TO POSITION\r\n"
#define OK_CUR_MOV_FOR				"223 OK CURSOR MOVED FORWARD\r\n"
#define OK_CUR_MOV_BACK				"224 OK CURSOR MOVED BACKWARD\r\n"
#define OK_MESSAGE_QUEUED			"225 OK MESSAGE QUEUED\r\n"
#define OK_SND_ICON_QUEUED			"226 OK SOUND ICON QUEUED\r\n"
#define OK_MSG_CANCELED				"227 OK MESSAGE CANCELED\r\n"

#define OK_RECEIVE_DATA				"230 OK RECEIVING DATA\r\n"
#define OK_BYE						"231 HAPPY HACKING\r\n"

#define OK_CLIENT_LIST_SENT			"240 OK CLIENTS LIST SENT\r\n"
	#define C_OK_CLIENTS				"240"
#define OK_MSGS_LIST_SENT			"241 OK MSGS LIST SENT\r\n"
	#define C_OK_MSGS					"241"
#define OK_LAST_MSG					"242 OK LAST MSG SAID\r\n"
	#define C_OK_LAST_MSG				"242"
#define OK_CUR_POS_RET				"243 OK CURSOR POSITION RETURNED\r\n"
	#define C_OK_CUR_POS				"243"

#define ERR_NO_CLIENT				"401 ERR NO CLIENT\r\n"
#define ERR_NO_SUCH_CLIENT			"402 ERR NO SUCH CLIENT\r\n"
#define ERR_NO_MESSAGE				"403 ERR NO MESSAGE\r\n"		
#define ERR_POS_LOW					"404 ERR POSITION TOO LOW\r\n"
#define ERR_POS_HIGH				"405 ERR POSITION TOO HIGH\r\n"
#define ERR_ID_NOT_EXIST			"406 ERR ID DOESNT EXIST\r\n"
#define ERR_UNKNOWN_ICON			"407 ERR UNKNOWN ICON\r\n"


#define ERR_INTERNAL				"300 ERR INTERNAL\r\n"
#define ERR_COULDNT_SET_PRIORITY	"301 ERR COULDNT SET PRIORITY\r\n"
#define ERR_COULDNT_SET_LANGUAGE	"302 ERR COULDNT SET LANGUAGE\r\n"
#define ERR_COULDNT_SET_RATE		"303 ERR COULDNT SET RATE\r\n"
#define ERR_COULDNT_SET_PITCH		"304 ERR COULDNT SET PITCH\r\n"
#define ERR_COULDNT_SET_PUNCT_MODE	"305 ERR COULDNT SET PUNCT MODE\r\n"
#define ERR_COULDNT_SET_CAP_LET_RECOG	"306 ERR COULDNT SET CAP LET RECOGNITION\r\n"
#define ERR_COULDNT_SET_PUNCT_MODE	"307 ERR COULDNT SET PUNCTUATION MODE\r\n"
#define ERR_COULDNT_SET_SPELLING	"308 ERR COULDNT SET SPELLING\r\n"
#define ERR_NO_SND_ICONS			"320 ERR NO SOUND ICONS\r\n"
#define ERR_NOT_IMPLEMENTED			"380 ERR NOT YET IMPLEMENTED\r\n"

#define ERR_INVALID_COMMAND			"500 ERR INVALID COMMAND\r\n"
#define ERR_MISSING_PARAMETER	   	"510 ERR MISSING PARAMETER\r\n"
#define ERR_NOT_A_NUMBER 		   	"511 ERR GIVEN PARAMETER NOT A NUMBER\r\n"
#define ERR_NOT_A_STRING 		   	"512 ERR GIVEN PARAMETER NOT A STRING\r\n"


