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
 * $Id: msg.h,v 1.3 2003-03-12 22:26:36 hanke Exp $
 */

#define ERR_INVALID_COMMAND		"ERR INVALID COMMAND\n\r"

#define ERR_NO_CLIENT			"ERR NO CLIENT\n\r"
#define ERR_NO_SUCH_CLIENT		"ERR NO SUCH CLIENT\n\r"
#define ERR_NO_MESSAGE			"ERR NO MESSAGE\n\r"			//TODO: ERR_NO_MSG
#define ERR_POS_LOW				"ERR POSITION TOO LOW\n\r"
#define ERR_POS_HIGH			"ERR POSITION TOO HIGH\n\r"
#define ERR_COULDNT_SET_PRIORITY	"ERR COULDNT SET PRIORITY\n\r"
#define ERR_COULDNT_SET_LANGUAGE	"ERR COULDNT SET LANGUAGE\n\r"
#define ERR_COULDNT_SET_RATE	"ERR COULDNT SET RATE\n\r"
#define ERR_COULDNT_SET_PITCH	"ERR COULDNT SET PITCH\n\r"
#define ERR_COULDNT_SET_PUNCT_MODE	"ERR COULDNT SET PUNCT MODE\n\r"
#define ERR_COULDNT_SET_CAP_LET_RECOG	"ERR COULDNT SET CAP LET RECOGNITION\n\r"
#define ERR_COULDNT_SET_PUNCT_MODE	"ERR COULDNT SET PUNCTUATION MODE\n\r"
#define ERR_COULDNT_SET_SPELLING	"ERR COULDNT SET SPELLING\n\r"
#define ERR_ID_NOT_EXIST		"ERR ID DOESNT EXIST\n\r"

#define ERR_NOT_A_NUMBER    	"ERR GIVEN PARAMETER NOT A NUMBER\n\r"

#define OK_LANGUAGE_SET			"OK LANGUAGE SET\n\r"
#define OK_PRIORITY_SET			"OK PRIORITY SET\n\r"
#define OK_RATE_SET				"OK RATE SET\n\r"
#define OK_PITCH_SET			"OK PITCH SET\n\r"
#define OK_PUNCT_MODE_SET		"OK PUNCTUATION SET\n\r"
#define OK_CAP_LET_RECOGN_SET	"OK CAP LET RECOGNITION SET\n\r"
#define OK_SPELLING_SET			"OK SPELLING SET\n\r"

#define OK_CUR_SET_FIRST		"OK CURSOR SET FIRST\n\r"
#define OK_CUR_SET_LAST			"OK CURSOR SET LAST\n\r"
#define OK_CUR_SET_POS			"OK CURSOR SET TO POSITION\n\r"
#define OK_CUR_MOV_FOR			"OK CURSOR MOVED FORWARD\n\r"
#define OK_CUR_MOV_BACK			"OK CURSOR MOVED BACKWARD\n\r"
#define OK_MESSAGE_QUEUED		"OK MESSAGE QUEUED\n\r"
#define OK_CLIENT_NAME_SET		"OK CLIENT NAME SET\n\r"
#define OK_RECEIVE_DATA				"OK RECEIVING DATA\n\r"
#define OK_		"\n\r"
#define OK_		"\n\r"
#define BYE_MSG				"HAPPY HACKING\n\r"
//#define OK_		"\n\r"


#define ERR_TOO_MUCH_DATA		"ERR TOO MUCH DATA\n\r"

