#
# test_module_readline.py - Python module_readline unit tests
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

import io
import os
import unittest

import speechd_python_modules.module_readline as module_readline


class ModuleReadlineTest(unittest.TestCase):
    def setUp(self):
        self._fds = []

    def tearDown(self):
        for fd in self._fds:
            module_readline._fd_buffers.pop(fd, None)
        for fd in reversed(self._fds):
            try:
                os.close(fd)
            except OSError:
                pass

    def pipe(self):
        read_fd, write_fd = os.pipe()
        self._fds.extend((read_fd, write_fd))
        return read_fd, write_fd

    def close_fd(self, fd):
        os.close(fd)
        self._fds.remove(fd)
        module_readline._fd_buffers.pop(fd, None)

    def test_nonblocking_empty_fd_returns_none(self):
        read_fd, _write_fd = self.pipe()

        self.assertIsNone(module_readline.module_readline(read_fd, block=False))

    def test_nonblocking_partial_line_is_buffered(self):
        read_fd, write_fd = self.pipe()

        os.write(write_fd, b"partial")
        self.assertIsNone(module_readline.module_readline(read_fd, block=False))

        os.write(write_fd, b"\nnext\n")
        self.assertEqual(
            module_readline.module_readline(read_fd, block=False),
            "partial\n",
        )
        self.assertEqual(
            module_readline.module_readline(read_fd, block=False),
            "next\n",
        )

    def test_eof_with_partial_line_returns_none(self):
        read_fd, write_fd = self.pipe()

        os.write(write_fd, b"partial")
        self.close_fd(write_fd)

        self.assertIsNone(module_readline.module_readline(read_fd, block=True))
        self.assertNotIn(read_fd, module_readline._fd_buffers)

    def test_invalid_utf8_round_trips_with_surrogateescape(self):
        read_fd, write_fd = self.pipe()

        os.write(write_fd, b"bad\xff\n")

        line = module_readline.module_readline(read_fd, block=True)
        self.assertEqual(line, "bad\udcff\n")
        self.assertEqual(line.encode("utf-8", "surrogateescape"), b"bad\xff\n")

    def test_file_like_source_uses_readline_fallback(self):
        source = io.StringIO("hello\n")

        self.assertEqual(module_readline.module_readline(source), "hello\n")

    def test_incomplete_file_like_line_returns_none(self):
        source = io.StringIO("partial")

        self.assertIsNone(module_readline.module_readline(source))

    def test_nonblocking_file_like_source_without_fd_returns_none(self):
        source = io.StringIO("hello\n")

        self.assertIsNone(module_readline.module_readline(source, block=False))


if __name__ == "__main__":
    unittest.main()
