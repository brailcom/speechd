#
# test_imports.py - Python module import tests
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
import unittest

from speechd_python_modules import module_readline, module_utils, speechd_types


class ImportsTest(unittest.TestCase):
    def test_modules_are_imported_from_package_directory(self):
        module_root = os.environ.get(
            "TEST_PYTHONPATH",
            os.path.join(os.path.dirname(__file__), "..", "..", "modules"),
        )
        package_dir = os.path.join(module_root, "speechd_python_modules")
        expected_dir = os.path.realpath(package_dir)
        modules = [module_readline, module_utils, speechd_types]

        for module in modules:
            with self.subTest(module=module.__name__):
                module_file = os.path.realpath(module.__file__)
                self.assertEqual(os.path.dirname(module_file), expected_dir)


if __name__ == "__main__":
    unittest.main()
