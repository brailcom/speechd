#
# test_module_utils.py - Python module_utils unit tests
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

import os
import tempfile
import unittest

from speechd_python_modules import module_utils


class ModuleUtilsTest(unittest.TestCase):
    def setUp(self):
        self.reset_debug_state()
        module_utils.log_level = 0

    def tearDown(self):
        self.reset_debug_state()
        module_utils.log_level = 0

    def reset_debug_state(self):
        if module_utils.CustomDebugFile is not None:
            module_utils.CustomDebugFile.close()
        module_utils.CustomDebugFile = None
        module_utils.Debug = 0

    def test_module_strip_ssml_removes_tags(self):
        strip_ssml = module_utils.module_strip_ssml

        self.assertEqual(strip_ssml("Plain text."), "Plain text.")
        self.assertEqual(
            strip_ssml("Hello <emphasis>world</emphasis>."),
            "Hello world.",
        )
        self.assertEqual(
            strip_ssml('<speak>Hello <break time="500ms"/>world</speak>'),
            "Hello world",
        )
        self.assertEqual(strip_ssml('<tag attr="&amp;">text</tag>'), "text")
        self.assertEqual(strip_ssml("Keep <unfinished"), "Keep ")

    def test_module_strip_ssml_decodes_known_entities(self):
        self.assertEqual(
            module_utils.module_strip_ssml(
                "Use &lt;tag&gt;, &amp;, &quot;quotes&quot; "
                "and &apos;apostrophes&apos;."
            ),
            "Use <tag>, &, \"quotes\" and 'apostrophes'.",
        )

    def test_module_strip_ssml_keeps_unknown_entities(self):
        self.assertEqual(
            module_utils.module_strip_ssml("Unknown &copy; entity stays."),
            "Unknown &copy; entity stays.",
        )

    def test_module_loglevel_set_accepts_numeric_prefix(self):
        self.assertEqual(module_utils.module_loglevel_set("log_level", "  -12xyz"), 0)
        self.assertEqual(module_utils.log_level, -12)

    def test_module_loglevel_set_rejects_wrong_item(self):
        module_utils.log_level = 4

        self.assertEqual(module_utils.module_loglevel_set("rate", "8"), -1)
        self.assertEqual(module_utils.log_level, 4)

    def test_module_loglevel_set_rejects_non_numeric_value(self):
        module_utils.log_level = 4

        self.assertEqual(module_utils.module_loglevel_set("log_level", "abc"), -1)
        self.assertEqual(module_utils.log_level, 4)

    def test_module_debug_enable_and_disable_custom_file(self):
        with tempfile.TemporaryDirectory() as directory:
            filename = os.path.join(directory, "debug.log")

            self.assertEqual(module_utils.module_debug(True, filename), 0)
            self.assertEqual(module_utils.Debug, 2)
            self.assertIsNotNone(module_utils.CustomDebugFile)
            self.assertFalse(module_utils.CustomDebugFile.closed)

            custom_debug_file = module_utils.CustomDebugFile
            self.assertEqual(module_utils.module_debug(False, None), 0)
            self.assertEqual(module_utils.Debug, 0)
            self.assertIsNone(module_utils.CustomDebugFile)
            self.assertTrue(custom_debug_file.closed)

    def test_module_debug_preserves_stdout_debug_state(self):
        with tempfile.TemporaryDirectory() as directory:
            filename = os.path.join(directory, "debug.log")
            module_utils.Debug = 1

            self.assertEqual(module_utils.module_debug(True, filename), 0)
            self.assertEqual(module_utils.Debug, 3)

            self.assertEqual(module_utils.module_debug(False, None), 0)
            self.assertEqual(module_utils.Debug, 1)

    def test_module_debug_closes_previous_file(self):
        with tempfile.TemporaryDirectory() as directory:
            first = os.path.join(directory, "first.log")
            second = os.path.join(directory, "second.log")

            self.assertEqual(module_utils.module_debug(True, first), 0)
            first_file = module_utils.CustomDebugFile
            self.assertEqual(module_utils.module_debug(True, second), 0)

            self.assertTrue(first_file.closed)
            self.assertFalse(module_utils.CustomDebugFile.closed)

    def test_module_debug_reports_open_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            filename = os.path.join(directory, "missing", "debug.log")

            self.assertEqual(module_utils.module_debug(True, filename), -1)
            self.assertEqual(module_utils.Debug, 0)
            self.assertIsNone(module_utils.CustomDebugFile)


if __name__ == "__main__":
    unittest.main()
