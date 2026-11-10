#
# module_utils.py - Utilities for Python output modules
#
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


import re


log_level = 0
Debug = 0
CustomDebugFile = None


def module_loglevel_set(cur_item, cur_value):
    global log_level

    if cur_item != "log_level":
        return -1

    match = re.match(r"\s*([+-]?[0-9]+)", cur_value)
    if match is None:
        return -1

    log_level = int(match.group(1), 10)
    return 0


# TODO Add an equivalent of C MSG() that writes to CustomDebugFile.
def module_debug(enable, filename):
    global CustomDebugFile, Debug

    if enable:
        try:
            new_custom_debug_file = open(filename, "w+")
        except OSError:
            return -1

        if CustomDebugFile is not None:
            CustomDebugFile.close()
        CustomDebugFile = new_custom_debug_file
        if Debug == 1:
            Debug = 3
        else:
            Debug = 2
    else:
        if Debug == 3:
            Debug = 1
        else:
            Debug = 0

        if CustomDebugFile is not None:
            CustomDebugFile.close()
        CustomDebugFile = None

    return 0


def module_strip_ssml(message: str) -> str:
    out = []
    append = out.append
    omit = False
    i = 0
    length = len(message)

    while i < length:
        char = message[i]

        if char == "<":
            omit = True
            i += 1
            continue
        if char == ">":
            omit = False
            i += 1
            continue
        if omit:
            i += 1
            continue

        if char == "&":
            if message.startswith("&lt;", i):
                append("<")
                i += 4
                continue
            if message.startswith("&gt;", i):
                append(">")
                i += 4
                continue
            if message.startswith("&amp;", i):
                append("&")
                i += 5
                continue
            if message.startswith("&quot;", i):
                append('"')
                i += 6
                continue
            if message.startswith("&apos;", i):
                append("'")
                i += 6
                continue

        append(char)
        i += 1

    return "".join(out)
