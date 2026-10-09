#
# test_speechd_types.py - Python speechd_types unit tests
#
# Copyright (C) 2026 Jean-François David
#
# This is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import unittest

from speechd_python_modules import speechd_types


class SpeechdTypesTest(unittest.TestCase):
    def test_constants(self):
        self.assertEqual(speechd_types.SPD_MSGTYPE_TEXT, 0)
        self.assertEqual(speechd_types.SPD_MSGTYPE_SPELL, 99)
        self.assertEqual(speechd_types.SPD_AUDIO_LE, 0)
        self.assertEqual(speechd_types.SPD_AUDIO_BE, 1)

    def test_spd_voice_fields(self):
        voice = speechd_types.SPDVoice(name="name", language="en", variant="male")

        self.assertEqual(voice.name, "name")
        self.assertEqual(voice.language, "en")
        self.assertEqual(voice.variant, "male")

    def test_audio_track_fields(self):
        track = speechd_types.AudioTrack(
            bits=16,
            num_channels=1,
            sample_rate=22050,
            num_samples=2,
            samples=b"\0\0\1\0",
        )

        self.assertEqual(track.bits, 16)
        self.assertEqual(track.num_channels, 1)
        self.assertEqual(track.sample_rate, 22050)
        self.assertEqual(track.num_samples, 2)
        self.assertEqual(track.samples, b"\0\0\1\0")


if __name__ == "__main__":
    unittest.main()
