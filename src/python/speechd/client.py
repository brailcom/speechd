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

"""Python API to Speech Daemon

Python client API to Speech Daemon is provided in a nice OO style by the class
called 'Client'.

"""

import socket
import string
from util import *

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
    """Speech Daemon client.

    This class implements most of the SSIP commands and provides OO style
    Python interface to all Speech Daemon functions.  Each connection to Speech
    Daemon server is represented by one instance of this class.  

    """
    
    SPEECH_PORT = 9876
    """Default port number for server connections."""
    
    RECV_BUFFER_SIZE = 1024
    """Size of buffer for receiving server responses."""

    SSIP_NEWLINE = "\r\n"
    """SSIP newline character sequence as a string."""
    
    _SSIP_END_OF_DATA = SSIP_NEWLINE + '.'  + SSIP_NEWLINE
    
    _SSIP_END_OF_DATA_ESCAPED = SSIP_NEWLINE + '..'  + SSIP_NEWLINE
    
    def __init__(self, user='joe', client='python', component='default',
                 host='127.0.0.1', port=SPEECH_PORT):
        """Initialize the instance and connect to the server.

        Arguments:

          user -- user identification string (user name)
          client -- client identification string
          component -- connection identification string
          host -- server hostname or IP address as string
          port -- server port as number (default value %d)
        
        For more information on client identification strings see Speech Daemod
        documentation.
          
        """ % self.SPEECH_PORT

        class _Socket(socket.socket):
            # Socket with a 'readline()' method.
            NEWLINE = self.SSIP_NEWLINE
            def readline(self):
                line = ''
                pointer = 0
                while 1:
                    char = self.recv(1)
                    line += char
                    if char == self.NEWLINE[pointer]:
                        pointer += 1
                    else:
                        pointer = 0                
                    if pointer == len(self.NEWLINE):
                        return line
                    
        self._socket = _Socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.connect((socket.gethostbyname(host), port))
        name = '%s:%s:%s' % (user, client, component)
        self._send_command('SET', 'self', 'CLIENT_NAME', name)
        
    def _send_command(self, command, *args):
        # Send command with given arguments and check server responsse.
        cmd = ' '.join((command,) + tuple(map(str, args))) + self.SSIP_NEWLINE
        self._socket.send(cmd)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise CommandError(code, msg, cmd)
        return code, msg, data

    def _send_data(self, data):
        # Send data and check server responsse.
        edata = string.replace(data,
                               self._SSIP_END_OF_DATA,
                               self._SSIP_END_OF_DATA_ESCAPED)
        self._socket.send(edata + self._SSIP_END_OF_DATA)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise SendDataError(code, msg, edata)
        return code, msg
        
    def _recv_response(self):
        # Read response from server; responses can be multiline as defined in
        # SSIP.  Return the tuple (code, msg, data), code as integer, msg as
        # string and data as tuple of strings (all lines).
        data = []
        c = None
        while 1:
            line = self._socket.readline()
            assert len(line) >= 4, "Malformed data received from server!"
            code, sep, text = line[:3], line[3], line[4:]
            assert code.isalnum() and (c is None or code == c) and \
                   sep in ('-', ' '), "Malformed data received from server!"
            if sep == ' ':
                msg = text
                return int(code), msg, tuple(data)
            data.append(text)
            
    def close(self):
        """Close the connection to Speech Daemon server."""
        #self._send_command('BYE')
        self._socket.close()
        
    def say(self, text, priority='message'):
        """Say given message with given priority.

        Arguments:

          text -- message text to be spoken as string.
          
          priority -- one of 'important', 'text', 'message', 'notification',
            'progress'.  For detailed description see Speech Daemon
            documentation.
        
        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        Server response code is checked and exception ('CommandError' or
        'SendDataError') is risen in case of non 2xx return code.  For more
        information about server responses and codes, see SSIP documentation.
        
        """
        self._send_command('SET', 'self', 'PRIORITY', priority)
        self._send_command('SPEAK')
        self._send_data(text)
        
    def stop(self, all=FALSE):
        """Immediately stop speaking and discard messages in queues.

        Arguments:

          all -- if true, all messages from all clients will be stopped and
            discarded.  If false (the default), only messages from this client
            will be affected.
        
        """
        if all:
            self._send_command('STOP', 'ALL')
        else:
            self._send_command('STOP')

    def get_client_list(self):
        code, msg, data = self._send_command('HISTORY', 'GET', 'CLIENT_LIST')
        return data
        
        
