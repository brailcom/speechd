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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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

    END_OF_DATA = NEWLINE + '.' + NEWLINE
    """Data end marker as a string."""
    
    END_OF_DATA_ESCAPED = NEWLINE + '..' + NEWLINE
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
    
    SPEECH_PORT = 9876
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
        self._conn.send_command('SET', 'self', 'PRIORITY', priority)
        self._conn.send_command('SPEAK')
        self._conn.send_data(text)
        
    def stop(self, all=FALSE):
        """Immediately stop speaking and discard messages in queues.

        Arguments:

          all -- if true, all messages from all clients will be stopped and
            discarded.  If false (the default), only messages from this client
            will be affected.
        
        """
        if all:
            self._conn.send_command('STOP', 'ALL')
        else:
            self._conn.send_command('STOP')


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
        assert scope in ('self', 'all') or type(scope) == type(0)
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
        assert scope in ('self', 'all') or type(scope) == type(0)
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
        assert scope in ('self', 'all') or type(scope) == type(0)
        self._conn.send_command('SET', scope, 'RATE', value)


    # VOICE, CAP_LET_RECOGN, PUNCTUATION
    
    def get_client_list(self):
        c, m, data = self._conn.send_command('HISTORY', 'GET', 'CLIENT_LIST')
        return data
        
        
    def close(self):
        """Close the connection to Speech Dispatcher."""
        #self._conn.send_command('BYE')
        self._conn.close()
