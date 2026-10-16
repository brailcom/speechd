#
# module_readline.py - Input buffering for Python output modules.
#
# Copyright (C) 2020 Samuel Thibault <samuel.thibault@ens-lyon.org>
# Copyright (C) 2026 Jean-François David <jeanfrancoismanutea@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

import os
import select
import sys


READ_CHUNK = 4096

_fd_buffers = {}


class _ReadBuffer:
    def __init__(self):
        self.data = bytearray()
        self.no_lf = 0

def module_readline(source=None, block=True):
    if source is None:
        source = sys.stdin
    if isinstance(source, int):
        return _readline_fd(source, block)

    fd = _source_fd(source)
    if fd is not None:
        return _readline_fd(fd, block)

    if not block:
        return None

    line = source.readline()
    return _decode_complete_line(line)


def _readline_fd(fd, block):
    state = _fd_buffers.get(fd)

    while True:
        if state is not None:
            newline = state.data.find(b"\n", state.no_lf)
            if newline != -1:
                line = bytes(state.data[: newline + 1])
                del state.data[: newline + 1]
                state.no_lf = 0
                if not state.data:
                    _fd_buffers.pop(fd, None)
                return _decode_bytes(line)

            state.no_lf = len(state.data)

        try:
            readable, _, _ = select.select([fd], [], [], None if block else 0)
        except (InterruptedError, BlockingIOError):
            if not block:
                return None
            continue
        except OSError:
            _fd_buffers.pop(fd, None)
            return None

        if not readable:
            return None

        try:
            chunk = os.read(fd, READ_CHUNK)
        except (InterruptedError, BlockingIOError):
            if not block:
                return None
            continue
        except OSError:
            _fd_buffers.pop(fd, None)
            return None

        if not chunk:
            if state is not None:
                _fd_buffers.pop(fd, None)
            return None

        if state is None:
            state = _ReadBuffer()
            _fd_buffers[fd] = state

        state.data.extend(chunk)


def _source_fd(source):
    try:
        return source.fileno()
    except (AttributeError, OSError, ValueError):
        return None


def _decode_complete_line(line):
    if not line:
        return None
    if not line.endswith(b"\n" if isinstance(line, bytes) else "\n"):
        return None
    if isinstance(line, bytes):
        return _decode_bytes(line)
    return line


def _decode_bytes(data):
    return data.decode("utf-8", "surrogateescape")
