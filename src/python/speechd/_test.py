#!/usr/bin/env python

# Copyright (C) 2003, 2006, 2007 Brailcom, o.p.s.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public Licensex1 as published by
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

import unittest
import time

from client import PunctuationMode, CallbackType, SSIPClient, Scope, Speaker

class TestSuite(unittest.TestSuite):
    def add(self, cls, prefix = 'check_'):
        tests = filter(lambda s: s[:len(prefix)] == prefix, dir(cls))
        self.addTest(unittest.TestSuite(map(cls, tests)))
tests = TestSuite()

class SSIPClientTest(unittest.TestCase):
    
    def _client(self):
        c = SSIPClient('test')
        c.set_language('en')
        c.set_rate(30)
        return c

    def check_escapes(self):
        c = self._client()
        try:
            c.speak("Testing data escapes:")
            c.set_punctuation(PunctuationMode.ALL)
            c.speak(".")
            c.speak("Marker at the end.\r\n.\r\n")
            c.speak(".\r\nMarker at the beginning.")
        finally:
            c.close()
        
    def check_voice_properties(self):
        c = self._client()
        try:
            c.speak("Testing voice properties:")
            c.set_pitch(-100)
            c.speak("I am fat Billy")
            c.set_pitch(100)
            c.speak("I am slim Willy")
            c.set_pitch(0)
            c.set_rate(100)
            c.speak("I am quick Dick.")
            c.set_rate(-80)
            c.speak("I am slow Joe.")
            c.set_rate(0)
            c.set_pitch(100)
            c.set_volume(-50)
            c.speak("I am quiet Mariette.")
            c.set_volume(100)
            c.speak("I am noisy Daisy.")
        finally:
            c.close()

    def check_other_commands(self):
        try:
            c = self._client()
            c.speak("Testing other commands:")
            c.char("a")
            c.key("shift_b")
            c.sound_icon("empty")
        finally:
            c.close()
        
    def check_callbacks(self):
        c = self._client()
        finished = []
        def callback(type, context):
            print "**", type, context
            if type in (CallbackType.CANCEL, CallbackType.END):
                finished.append(context)
        try:
            c.speak("Message 1", callback=lambda type: callback(type, 'msg1'))
            c.speak("Message 2", callback=lambda type: callback(type, 'msg2'))
            time.sleep(2)
            print "= CANCEL ="
            c.cancel(Scope.ALL)
            assert 'msg1' in finished and 'msg2' in finished, finished
        finally:
            c.close()

#    def check_notification(self):
#         def my_callback(msg_id, client_id, ctype, index_mark=None):            
#             print "  Callback received on message " + str(msg_id) + \
#                   " from client " + str(client_id) \
#                   + ": " + ctype
#             if index_mark:
#                 print "    Index mark name: " + index_mark

#         c = self._client()
#         print "  Cancel all previous messages"
#         c.cancel(Scope.ALL)
#         c.set_notification(my_callback, (CallbackType.BEGIN, CallbackType.END))
#         c.speak("""Informationabout the beginning and the end of this message
#         should appear on the screen""")
#         print "  Waiting for the speech to start and finish in 10 seconds"
#         time.sleep(10)
#         print "End of callback test"
    

tests.add(SSIPClientTest)

class SpeakerTest(unittest.TestCase):
    pass
    #Commented out. This is not implemented yet and there is no spec.
    
    #def check_it(self):
    #    s = Speaker('test')
    #    s.set_language('en')
    #    #s.set_rate(30)
    #    s.say("Testing the say command.")
        
tests.add(SpeakerTest)

def get_tests():
    return tests

if __name__ == '__main__':
    unittest.main(defaultTest='get_tests')
