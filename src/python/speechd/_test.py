#!/usr/bin/env python

# Copyright (C) 2003 Brailcom, o.p.s.
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import unittest

import client

class TestSuite(unittest.TestSuite):
    def add(self, cls, prefix = 'check_'):
        tests = filter(lambda s: s[:len(prefix)] == prefix, dir(cls))
        self.addTest(unittest.TestSuite(map(cls, tests)))
tests = TestSuite()

class Client(unittest.TestCase):
    def check_it(self):
        c = client.Client('bill', 'xxx', 'yyy')
        c.set_rate(30)
        c.set_language('en')
        c.say("Hello, this is a Python client test.")
        c.set_pitch(-100)
        c.say("I am fat Billy")
        c.set_pitch(100)
        c.say("I am slim Willy")
        c.set_pitch(0)
        c.set_rate(100)
        c.say("I am quick Dick.")
        c.set_rate(-100)
        c.say("I am slow Joe.")
        c.close()
tests.add(Client)

def get_tests():
    return tests

if __name__ == '__main__':
    unittest.main(defaultTest='get_tests')
