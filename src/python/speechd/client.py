# Copyright (C) 2003-2006 Brailcom, o.p.s.
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

Basic Python client API to Speech Dispatcher is provided by the 'SSIPClient'
class.  This interface maps directly to available SSIP commands and logic.

A more convenient interface is provided by the 'Speaker' class.

"""

import socket

class SSIPError(Exception):
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

    
class SSIPCommandError(SSIPError):
    """Exception raised on error response after sending command."""

    def command(self):
        """Return the command string which resulted in this error."""
        return self._data

    
class SSIPDataError(SSIPError):
    """Exception raised on error response after sending data."""

    def data(self):
        """Return the data which resulted in this error."""
        return self._data


class _SSIP_Connection:
    """Implemantation of low level SSIP communication."""
    
    _NEWLINE = "\r\n"
    """New line delimeter as a string."""

    _END_OF_DATA_MARKER = '.'
    """Data end marker."""

    _END_OF_DATA_MARKER_ESCAPED = '..'
    """Escaped data end marker."""
    
    _END_OF_DATA = _NEWLINE + _END_OF_DATA_MARKER + _NEWLINE
    """Data end marker."""

    _END_OF_DATA_ESCAPED = _NEWLINE + _END_OF_DATA_MARKER_ESCAPED + _NEWLINE
    """Data may contain a marker string, so we need to escape it..."""

    def __init__(self, host, port):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.connect((socket.gethostbyname(host), port))
        self._buffer = ""
    
    def _readline(self):
        """Read one whole line from the socket.

        Blocks until the line delimiter ('_NEWLINE') is read.
        
        """
        pointer = self._buffer.find(self._NEWLINE)
        while pointer == -1:
            self._buffer += self._socket.recv(1024)
            pointer = self._buffer.find(self._NEWLINE)
        line = self._buffer[:pointer]
        self._buffer = self._buffer[pointer+len(self._NEWLINE):]
        return line
            
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
        
        An 'SSIPCommandError' exception is raised in case of non 2xx return
        code.  For more information about server responses and codes, see SSIP
        documentation.
       
        """
        if __debug__:
            if command in ('SET', 'CANCEL', 'STOP',):
                assert args[0] in (Scope.SELF, Scope.ALL), scope
                #or isinstance(args[0], int)
        cmd = ' '.join((command,) + tuple(map(str, args)))
        self._socket.send(cmd + self._NEWLINE)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise SSIPCommandError(code, msg, cmd)
        return code, msg, data
        
    def send_data(self, data):
        """Send multiline data and read server responsse.

        Returned value is the same as for 'send_command()' method.

        An 'SSIPCommandError' exception is raised in case of non 2xx return
        code.  For more information about server responses and codes, see SSIP
        documentation.
        
        """
        # Escape the end-of-data marker even if present at the beginning
        if data.startswith(self._END_OF_DATA_MARKER + self._NEWLINE):
            l = len(self._END_OF_DATA_MARKER)
            data = self._END_OF_DATA_MARKER_ESCAPED + data[l:]
        elif data == self._END_OF_DATA_MARKER:
            data = self._END_OF_DATA_MARKER_ESCAPED
        data = data.replace(self._END_OF_DATA, self._END_OF_DATA_ESCAPED)
        self._socket.send(data + self._END_OF_DATA)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise SSIPDataError(code, msg, data)
        return code, msg
        
    def close(self):
        """Close the connection."""
        self._socket.close()
        self._socket = None
            

class Scope(object):
    """An enumeration of valid SSIP command scopes.

    The constants of this class should be used to specify the 'scope' argument
    for the 'Client' methods.

    """    
    SELF = 'self'
    """The command (mostly a setting) applies to current connection only."""
    ALL = 'all'
    """The command applies to all current Speech Dispatcher connections."""

    
class Priority(object):
    """An enumeration of valid SSIP message priorities.

    The constants of this class should be used to specify the 'priority'
    argument for the 'Client' methods.  For more information about message
    priorities and their interaction, see the SSIP documentation.
    
    """
    IMPORTANT = 'important'
    TEXT = 'text'
    MESSAGE = 'message'
    NOTIFICATION = 'notification'
    PROGRESS = 'progress'

    
class PunctuationMode(object):
    """Constants for selecting a punctuation mode.

    The mode determines which characters should be read.

    """
    ALL = 'all'
    """Read all punctuation characters."""
    NONE = 'none'
    """Don't read any punctuation character at all."""
    SOME = 'some'
    """Only the user-defined punctuation characters are read.

    The set of characters is specified in Speech Dispatcher configuration.

    """

    
class SSIPClient(object):
    """Basic Speech Dispatcher client interface.

    This class provides a Python interface to Speech Dispatcher functionality
    over an SSIP connection.  The API maps directly to available SSIP commands.
    Each connection to Speech Dispatcher is represented by one instance of this
    class.
    
    Many commands take the 'scope' argument, thus it is shortly documented
    here.  It is either one of 'Scope' constants or a number of connection.  By
    specifying the connection number, you are applying the command to a
    particular connection.  This feature is only meant to be used by Speech
    Dispatcher control application, however.  More datails can be found in
    Speech Dispatcher documentation.

    """
    
    SPEECH_PORT = 6560
    """Default port number for server connections."""
    
    def __init__(self, name, component='default', user='unknown',
                 host='127.0.0.1', port=None):
        """Initialize the instance and connect to the server.

        Arguments:

          name -- client identification string

          component -- connection identification string.  When one client opens
            multiple connections, this can be used to identify each of them.
            
          user -- user identification string (user name).  When multi-user
            acces is expected, this can be used to identify their connections.
          
          host -- server hostname or IP address as a string.
          
          port -- server port as number or None.  If None, the default value is
            determined by the SPEECH_PORT attribute of this class.
        
        For more information on client identification strings see Speech
        Dispatcher documentation.
          
        """
        self._conn = _SSIP_Connection(host, port or self.SPEECH_PORT)
        full_name = '%s:%s:%s' % (user, name, component)
        self._conn.send_command('SET', Scope.SELF, 'CLIENT_NAME', full_name)

    def set_priority(self, priority):
        """Set the priority category for the following messages.

        Arguments:

          priority -- one of the 'Priority' constants.

        """
        assert priority in (Priority.IMPORTANT, Priority.MESSAGE,
                            Priority.TEXT, Priority.NOTIFICATION,
                            Priority.PROGRESS), priority
        self._conn.send_command('SET', Scope.SELF, 'PRIORITY', priority)

    def speak(self, text):
        """Say given message.

        Arguments:

          text -- message text to be spoken as string.
          
        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        """
        self._conn.send_command('SPEAK')
        self._conn.send_data(text)

    def char(self, char):
        """Say given character.

        Arguments:

          char -- a character to be spoken (as a unicode string of length 1).

        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        """
        self._conn.send_command('CHAR', char.replace(' ', 'space'))
        
    def key(self, key):
        """Say given key name.

        Arguments:

          key -- the key name (as defined in SSIP).

        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        """
        self._conn.send_command('KEY', key)

    def sound_icon(self, sound_icon):
        """Output given sound_icon.

        Arguments:

          sound_icon -- the name of the sound icon as defined by SSIP.

        This method is non-blocking; it just sends the command, given message
        is queued on the server and the method returns immediately.

        """        
        self._conn.send_command('SOUND_ICON', sound_icon)
                    
    def cancel(self, scope=Scope.SELF):
        """Immediately stop speaking and discard messages in queues.

        Arguments:

          scope -- see the documentaion of this class.
            
        """
        self._conn.send_command('CANCEL', scope)


    def stop(self, scope=Scope.SELF):
        """Immediately stop speaking the currently spoken message.

        Arguments:

          scope -- see the documentaion of this class.
        
        """
        self._conn.send_command('STOP', scope)

    def pause(self, scope=Scope.SELF):
        """Pause speaking and postpone other messages until resume.

        This method is non-blocking.  However, speaking can continue for a
        short while even after it's called (typically to the end of the
        sentence).

        Arguments:

          scope -- see the documentaion of this class.
        
        """
        self._conn.send_command('STOP', scope)

    def resume(self, scope=Scope.SELF):
        """Resume speaking of the currently paused messages.

        This method is non-blocking.  However, speaking can continue for a
        short while even after it's called (typically to the end of the
        sentence).

        Arguments:

          scope -- see the documentaion of this class.
        
        """
        self._conn.send_command('STOP', scope)

    def set_language(self, language, scope=Scope.SELF):
        """Switch to a particular language for further speech commands.

        Arguments:

          language -- two letter language code according to RFC 1776 as string.

          scope -- see the documentaion of this class.
            
        """
        assert isinstance(language, str) and len(language) == 2
        self._conn.send_command('SET', scope, 'LANGUAGE', language)

    def set_pitch(self, value, scope=Scope.SELF):
        """Set the pitch for further speech commands.

        Arguments:

          value -- integer value within the range from -100 to 100, with 0
            corresponding to the default pitch of the current speech synthesis
            output module, lower values meaning lower pitch and higher values
            meaning higher pitch.

          scope -- see the documentaion of this class.
          
        """
        assert isinstance(value, int) and -100 <= value <= 100, value
        self._conn.send_command('SET', scope, 'PITCH', value)

    def set_rate(self, value, scope=Scope.SELF):
        """Set the speech rate (speed) for further speech commands.

        Arguments:

          value -- integer value within the range from -100 to 100, with 0
            corresponding to the default speech rate of the current speech
            synthesis output module, lower values meaning slower speech and
            higher values meaning faster speech.

          scope -- see the documentaion of this class.
            
        """
        assert isinstance(value, int) and -100 <= value <= 100
        self._conn.send_command('SET', scope, 'RATE', value)

    def set_volume(self, value, scope=Scope.SELF):
        """Set the speech volume for further speech commands.

        Arguments:

          value -- integer value within the range from -100 to 100, with 100
            corresponding to the default speech volume of the current speech
            synthesis output module, lower values meaning softer speech.

          scope -- see the documentaion of this class.
            
        """
        assert isinstance(value, int) and -100 <= value <= 100
        self._conn.send_command('SET', scope, 'VOLUME', value)

    def set_punctuation(self, value, scope=Scope.SELF):
        """Set the punctuation pronounciation level.

        Arguments:

          value -- one of the 'PunctuationMode' constants.

          scope -- see the documentaion of this class.
            
        """
        assert value in (PunctuationMode.ALL, PunctuationMode.SOME,
                         PunctuationMode.NONE), value
        self._conn.send_command('SET', scope, 'PUNCTUATION', value)

    def set_spelling(self, value, scope=Scope.SELF):
        """Toogle the spelling mode or on off.

        Arguments:

          value -- if 'True', all incomming messages will be spelled
            instead of being read as normal words. 'False' switches
            this behavior off.

          scope -- see the documentaion of this class.
            
        """
        assert value in [True, False]
        if value == True:
            self._conn.send_command('SET', scope, 'SPELLING', "on")
        else:
            self._conn.send_command('SET', scope, 'SPELLING', "off")

    def set_cap_let_recogn(self, value, scope=Scope.SELF):
        """Set capital letter recognition mode.

        Arguments:

          value -- one of 'none', 'spell', 'icon'. None means no signalization
            of capital letters, 'spell' means capital letters will be spelled
            with a syntetic voice and 'icon' means that the capital-letter icon
            will be prepended before each capital letter.

          scope -- see the documentaion of this class.
            
        """
        assert value in ("none", "spell", "icon")
        self._conn.send_command('SET', scope, 'CAP_LET_RECOGN', value)

    def set_voice(self, value, scope=Scope.SELF):
        """Set voice to given value.

        Note that this doesn't guarantee that such a voice is actually
        available by the used synthesizer.  If not, the suggested voice will be
        mapped to one of the available voices.

        Arguments:

          value -- one of the SSIP symbolic voice names: 'MALE1' .. 'MALE3',
            'FEMALE1' ... 'FEMALE3', 'CHILD_MALE', 'CHILD_FEMALE'

          scope -- see the documentaion of this class.
            
        """
        assert isinstance(value, str) and \
               value.lower() in ("male1", "male2", "male3", "female1",
                                 "female2", "female3", "child_male",
                                 "child_female")
        self._conn.send_command('SET', scope, 'VOICE', value)

    def set_pause_context(self, value, scope=Scope.SELF):
        """Set the amount of context when resuming a paused message.

        Arguments:

          value -- a positive or negative value meaning how many chunks of data
            after or before the pause should be read when resume() is executed.

          scope -- see the documentaion of this class.
            
        """
        assert isinstance(value, int)
        self._conn.send_command('SET', scope, 'PAUSE_CONTEXT', value)

    def block_begin(self):
        """Begin an SSIP block.

        See SSIP documentation for more details about blocks.

        """
        self._conn.send_command('BLOCK', 'BEGIN')

    def block_end(self):
        """Close an SSIP block.

        See SSIP documentation for more details about blocks.

        """
        self._conn.send_command('BLOCK', 'END')
        
    def close(self):
        """Close the connection to Speech Dispatcher."""
        self._conn.close()


class Client(SSIPClient):
    """A DEPRECATED backwards-compatible API.

    This Class is provided only for backwards compatibility with the prevoius
    unofficial API.  It will be removed in future versions.  Please use either
    'SSIPClient' or 'Speaker' interface instead.  As deprecated, the API is no
    longer documented.

    """
    def __init__(self, name=None, client=None, **kwargs):
        name = name or client or 'python'
        super(Client, self).__init__(name, **kwargs)
        
    def say(self, text, priority=Priority.MESSAGE):
        self.set_priority(priority)
        self.speak(text)

    def char(self, char, priority=Priority.TEXT):
        self.set_priority(priority)
        super(Client, self).char(char)

    def key(self, key, priority=Priority.TEXT):
        self.set_priority(priority)
        super(Client, self).key(key)

    def sound_icon(self, sound_icon, priority=Priority.TEXT):
        self.set_priority(priority)
        super(Client, self).sound_icon(sound_icon)
        

class Speaker(SSIPClient):
    """Extended Speech Dispatcher Interface.

    This class provides an extended intercace to Speech Dispatcher
    functionality and tries to hide most of the lower level details of SSIP
    (such as a more sophisticated handling of blocks and priorities and
    advanced event notifications) under a more convenient API.
    
    Please note that the API is not yet stabilized and thus is subject to
    change!  Please contact the authors if you plan using it and/or if you have
    any suggestions.

    Well, in fact this class is currently not implemented at all.  It is just a
    draft.  The intention is to hide the SSIP details and provide a generic
    interface practical for screen readers.
    """
