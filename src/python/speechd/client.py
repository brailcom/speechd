# Copyright (C) 2003 Brailcom, o.p.s.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

"""Python API to Speech Dispatcher

Python client API to Speech Dispatcher is provided in a nice OO style by the class
called 'Client'.

"""

import socket
import string
from util import *

class _SSIP_Connection:
    
    """Implemantation of low level SSIP communication."""
    
    NEWLINE = "\r\n"
    """New line delimeter as a string."""

    END_OF_DATA_SINGLE = '.'
    """Data end marker single in a message"""

    END_OF_DATA_BEGIN = END_OF_DATA_SINGLE + NEWLINE
    """Data end marker on the begginning of a message"""

    END_OF_DATA = NEWLINE + END_OF_DATA_BEGIN
    """Data end marker as a string."""


    END_OF_DATA_ESCAPED_SINGLE = '..'
    """Data may contain a marker string, so we need to escape it..."""

    END_OF_DATA_ESCAPED_BEGIN = END_OF_DATA_ESCAPED_SINGLE + NEWLINE
    """Data may contain a marker string, so we need to escape it..."""
    
    END_OF_DATA_ESCAPED = NEWLINE + END_OF_DATA_ESCAPED_BEGIN
    """Data may contain a marker string, so we need to escape it..."""

    def __init__(self, host, port):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.connect((socket.gethostbyname(host), port))

    
    def _readline(self):
        """Read one whole line from the socket.
        
        Read from the socket until the newline delimeter (given by the
        `NEWLINE' constant).  Blocks until the delimiter is read.
        
        """
        line = ''
        pointer = 0
        while 1:
            char = self._socket.recv(1)
            line += char
            if char == self.NEWLINE[pointer]:
                pointer += 1
            else:
                pointer = 0
            if pointer == len(self.NEWLINE):
                return line[:-pointer]
            
    def _recv_response(self):
        """Read server response and return the triplet (code, msg, data)."""
        data = []
        c = None
        while 1:
            line = self._readline()
            assert len(line) >= 4, "Malformed data received from server!"
            code, sep, text = line[:3], line[3], line[4:]
            assert code.isalnum() and (c is None or code == c) and \
                   sep in ('-', ' '), "Malformed data received from server!"
            if sep == ' ':
                msg = text
                return int(code), msg, tuple(data)
            data.append(text)

    def send_command(self, command, *args):
        """Send SSIP command with given arguments and read server responsse.

        Arguments can be of any data type -- they are all stringified before
        being sent to the server.

        Return a triplet (code, msg, data), where 'code' is a numeric SSIP
        response code as an integer, 'msg' is an SSIP rsponse message as string
        and 'data' is a tuple of strings (all lines of response data) when a
        response contains some data.
        
        Raise an exception in case of error response (non 2xx code).
        
        """
        cmd = ' '.join((command,) + tuple(map(str, args)))
        self._socket.send(cmd + self.NEWLINE)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise CommandError(code, msg, cmd)
        return code, msg, data

    def send_data(self, data):
        """Send multiline data and read server responsse.

        Returned value is the same as for 'send_command()' method.

        """

        # Escape the end-of-data sequence even if presented on the beginning
        if data[0:3] == self.END_OF_DATA_BEGIN:
            data = self.END_OF_DATA_ESCAPED_BEGIN + data[3:]

        if (len(data) == 1) and (data[0] == '.'):
            data = self.END_OF_DATA_ESCAPED_SINGLE
        
        data = string.replace(data, self.END_OF_DATA, self.END_OF_DATA_ESCAPED)
        self._socket.send(data + self.END_OF_DATA)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise SendDataError(code, msg, data)
        return code, msg
        
    def close(self):
        """Close the connection."""
        self._socket.close()
            

class _CommunicationError(Exception):
    def __init__(self, code, msg, data):
        Exception.__init__(self, "%s: %s" % (code, msg))
        self._code = code
        self._msg = msg
        self._data = data

    def code(self):
        """Return the server response error code as integer number."""
        return self._code
        
    def msg(self):
        """Return server response error message as string."""
        return self._msg

    
class CommandError(_CommunicationError):
    """Exception risen after receiving error response when command was sent."""

    def command(self):
        """Return the command string which resulted in this error."""
        return self._data

    
class SendDataError(_CommunicationError):
    """Exception risen after receiving error response when data were sent."""

    def data(self):
        """Return the data which resulted in this error."""
        return self._data
        
        
class Client:
    """Speech Dispatcher client.

    This class implements most of the SSIP commands and provides OO style
    Python interface to all Speech Dispatcher functions.  Each connection to
    Speech Dispatcher is represented by one instance of this class.

    """
    
    SPEECH_PORT = 6560
    """Default port number for server connections."""
    
    def __init__(self, user='joe', client='python', component='default',
                 host='127.0.0.1', port=SPEECH_PORT):
        """Initialize the instance and connect to the server.

        Arguments:

          user -- user identification string (user name)
          client -- client identification string
          component -- connection identification string
          host -- server hostname or IP address as string
          port -- server port as number (default value %d)
        
        For more information on client identification strings see Speech
        Dispatcher documentation.
          
        """ % self.SPEECH_PORT
        self._conn = _SSIP_Connection(host, port)
        name = '%s:%s:%s' % (user, client, component)
        self._conn.send_command('SET', 'self', 'CLIENT_NAME', name)

    def __set_priority(self, priority):
        """Set the given priority

        Arguments:
          priority -- one of 'important', 'text', 'message', 'notification',
            'progress'.  For detailed description see Speech Dispatcher
            documentation.

        This function is considered internal only. See say() or say_character()
        for information how to specify priorities of messages.
        """
        
        self._conn.send_command('SET', 'self', 'PRIORITY', priority)

    def __check_scope(self, scope):
        assert scope in ('self', 'all') or type(scope) == type(0)

    def say(self, text, priority='message'):
        """Say given message with given priority.

        Arguments:

          text -- message text to be spoken as string.
          
          priority -- one of 'important', 'text', 'message', 'notification',
            'progress'.  For detailed description see Speech Dispatcher
            documentation.
        
        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        Server response code is checked and exception ('CommandError' or
        'SendDataError') is risen in case of non 2xx return code.  For more
        information about server responses and codes, see SSIP documentation.
        
        """
        self.__set_priority(priority)
        self._conn.send_command('SPEAK')
        self._conn.send_data(text)

    def char(self, char, priority='text'):
        """Say given character with given priority

        Arguments:

          char -- character to be spoken in long UTF-8

          priority -- one of 'important', 'text', 'message', 'notification',
            'progress'.  For detailed description see Speech Dispatcher
            documentation.

        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        Server response code is checked and exception ('CommandError' or
        'SendDataError') is risen in case of non 2xx return code.  For more
        information about server responses and codes, see SSIP documentation.
        """

        self.__set_priority(priority)

        if char != " ":
            self._conn.send_command('CHAR', char)
        else:
            self._conn.send_command('CHAR', 'space')

    def key(self, key, priority='text'):
        """Say given character with given priority

        Arguments:

          key -- the key-name (as defined in SSIP) of the key to be spoken.
             For example: 'a', 'A', 'shift_a', 'shift_kp-enter', 'ctrl-alt-del'.

          priority -- one of 'important', 'text', 'message', 'notification',
            'progress'.  For detailed description see SSIP documentation.

        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        Server response code is checked and exception ('CommandError' or
        'SendDataError') is risen in case of non 2xx return code.  For more
        information about server responses and codes, see SSIP documentation.
        """
        self.__set_priority(priority)
        self._conn.send_command('KEY', key)

    def sound_icon(self, sound_icon, priority='text'):
        """Say or play given sound_icon with given priority

        Arguments:

          sound_icon -- the name of the sound icon.
             For example: 'empty', 'bell', 'new-line'.

          priority -- one of 'important', 'text', 'message', 'notification',
            'progress'.  For detailed description see SSIP documentation.

        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        Server response code is checked and exception ('CommandError' or
        'SendDataError') is risen in case of non 2xx return code.  For more
        information about server responses and codes, see SSIP documentation.
        """        
        self.__set_priority(priority)
        self._conn.send_command('SOUND_ICON', sound_icon)
                    
    def cancel(self, scope='self'):
        """Immediately stop speaking and discard messages in queues.

        Arguments:

          scope -- either a string constant 'self' or 'all' or a number of
            connection.  'self' means, that this setting applies to this
            connection only.  'all' changes the setting for all current Speech
            Dispatcher connections.  You can also set the value for any
            particular connection by supplying its identification number (this
            feature is only meant to be used by Speech Dispatcher control
            application).
        """
        self.__check_scope(scope)
        self._conn.send_command('CANCEL', scope)


    def stop(self, scope='self'):
        """Immediately stop speaking the currently spoken message.

        Arguments:

          scope -- as described in cancel.
        
        """
        self.__check_scope(scope)
        self._conn.send_command('STOP', scope)

    def pause(self, scope='self'):
        """Pause speaking the currently spoken message and postpone other messages
        until the resume method is called. This method is non-blocking. However,
        speaking can continue for a short while even after it's called (typically
        to the end of the sentence).

        Arguments:

          scope -- as described in cancel.
        
        """
        self.__check_scope(scope)
        self._conn.send_command('STOP', scope)

    def resume(self, scope='self'):
        """Resume speaking of the currently paused messages until the
        resume method is called. This method is non-blocking. However,
        speaking can continue for a short while even after it's called
        (typically to the end of the sentence).

        Arguments:

          scope -- as described in cancel.
        
        """
        self.__check_scope(scope)
        self._conn.send_command('STOP', scope)

    def set_language(self, language, scope='self'):
        """Switch to a particular language for further speech commands.

        Arguments:

          language -- two letter language code according to RFC 1776 as string.

          scope -- either a string constant 'self' or 'all' or a number of
            connection.  'self' means, that this setting applies to this
            connection only.  'all' changes the setting for all current Speech
            Dispatcher connections.  You can also set the value for any
            particular connection by supplying its identification number (this
            feature is only meant to be used by Speech Dispatcher control
            application).
            
        """
        assert type(language) == type('') and len(language) == 2
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'LANGUAGE', language)

    def set_pitch(self, value, scope='self'):
        """Set the pitch for further speech commands.

        Arguments:

          value -- integer value within the range from -100 to 100, with 0
            corresponding to the default pitch of the current speech synthesis
            output module, lower values meaning lower pitch and higher values
            meaning higher pitch.
          scope -- as described in 'set_language()'.
            
        """
        assert type(value) == type(0) and value >= -100 and value <= 100
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'PITCH', value)

    def set_rate(self, value, scope='self'):
        """Set the speech rate (speed) for further speech commands.

        Arguments:

          value -- integer value within the range from -100 to 100, with 0
            corresponding to the default speech rate of the current speech
            synthesis output module, lower values meaning slower speech and
            higher values meaning faster speech.

          scope -- as described in 'set_language()'.
            
        """
        assert type(value) == type(0) and value >= -100 and value <= 100
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'RATE', value)

    def set_volume(self, value, scope='self'):
        """Set the speech volume for further speech commands.

        Arguments:

          value -- integer value within the range from -100 to 100, with 100
            corresponding to the default speech volume of the current speech
            synthesis output module, lower values meaning softer speech.

          scope -- as described in 'set_language()'.
            
        """
        assert type(value) == type(0) and value >= -100 and value <= 100
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'VOLUME', value)

    def set_punctuation(self, value, scope='self'):
        """Set the punctuation pronounciation level.

        Arguments:

          value -- choses how much punctuation characters should be read.
            Possible values are: 'all', 'some', 'none'. 'all' means read
            all punctuation characters, while 'none' means that no punctuation
            character will be read. For 'some', only the user-defined punctuation
            characters are pronounced.

          scope -- as described in 'set_language()'.
            
        """
        assert value in ['all', 'some', 'none']
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'PUNCTUATION', value)

    def set_spelling(self, value, scope='self'):
        """Toogle the spelling mode or on off.

        Arguments:

          value -- if 'True', all incomming messages will be spelled
            instead of being read as normal words. 'False' switches
            this behavior off.
          scope -- as described in 'set_language()'.
            
        """
        assert value in [True, False]
        self.__check_scope(scope)
        if value == True:
            self._conn.send_command('SET', scope, 'SPELLING', "on")
        else:
            self._conn.send_command('SET', scope, 'SPELLING', "off")

    def set_cap_let_recogn(self, value, scope='self'):
        """Set capital letter recognition mode

        Arguments:

          value -- one of 'none', 'spell', 'icon'. None means no signalization
            of capital letters, 'spell' means capital letters will be spelled with
            a syntetic voice and 'icon' means that the capital-letter icon will be
            prepended before each capital letter.

          scope -- as described in 'set_language()'.
            
        """
        assert value in ["none", "spell", "icon"]
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'CAP_LET_RECOGN', value)

    def set_voice(self, value, scope='self'):
        """Set voice to given value. Note that this doesn't guarantee that
        such a voice is actually available by the used synthesizer. If not,
        the suggested voice will be mapped to one of the available voices.

        Arguments:

          value -- one of the SSIP symbolic voice names: 'MALE1' .. 'MALE3',
            'FEMALE1' ... 'FEMALE3', 'CHILD_MALE', 'CHILD_FEMALE'

          scope -- as described in 'set_language()'.
            
        """
        assert type(value) == type("t")
        assert value.lower() in ["male1", "male2", "male3", "female1", "female2",
                                 "female3", "child_male", "child_female"]
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'VOICE', value)

    def set_pause_context(self, value, scope='self'):
        """Set the amount of context that is provided to the user after resuming
        a paused message to the given value.

        Arguments:

          value -- a positive or negative value meaning how many chunks of data
          after or before the pause should be read when resume() is executed.

          scope -- as described in 'set_language()'.
            
        """
        assert type(value) == type(-1)
        self.__check_scope(scope)
        self._conn.send_command('SET', scope, 'PAUSE_CONTEXT', value)

    def block_begin(self):
        """Begin an SSIP block of messages. See SSIP documentation for more
        details about what are blocks and how to use them."""

        self._conn.send_command('BLOCK', 'BEGIN')

    def block_end(self):
        """Close an SSIP block of messages. See SSIP documentation for more
        details about what are blocks and how to use them."""

        self._conn.send_command('BLOCK', 'END')
        
    
#    def get_client_list(self):
#        c, m, data = self._conn.send_command('HISTORY', 'GET', 'CLIENT_LIST')
#        return data
        
        
    def close(self):
        """Close the connection to Speech Dispatcher."""
        #self._conn.send_command('BYE')
        self._conn.close()
