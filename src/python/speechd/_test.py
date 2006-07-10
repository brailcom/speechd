#!/usr/bin/env python

# Copyright (C) 2003, 2006 Brailcom, o.p.s.
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

from client import PunctuationMode, SSIPClient, Speaker

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
        c.speak("Testing data escapes:")
        c.set_punctuation(PunctuationMode.ALL)
        c.speak(".")
        c.speak("Marker at the end.\r\n.\r\n")
        c.speak(".\r\nMarker at the beginning.")
        
    def check_voice_properties(self):
        c = self._client()
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

    def check_other_commands(self):
        c = self._client()
        c.speak("Testing other commands:")
        c.char("a")
        c.key("shift_b")
        c.sound_icon("empty")
        
tests.add(SSIPClientTest)

class SpeakerTest(unittest.TestCase):
    
    def check_it(self):
        s = Speaker('test')
        s.set_language('en')
        #s.set_rate(30)
        s.say("Testing the say command.")
        
tests.add(SpeakerTest)

def get_tests():
    return tests

if __name__ == '__main__':
    unittest.main(defaultTest='get_tests')
