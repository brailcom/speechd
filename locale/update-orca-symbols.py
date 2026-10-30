#!/usr/bin/env python3
# coding: utf-8
# Orca symbol dictionaries update
# Remove symbols which are part of NVDA symbols from Orca dictionaries
# Copyright (C) 2001, 2026 Patrick Zajda
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
###

import os
from pathlib import Path
import stat
import tempfile


ORCA_DICTIONARIES = ("orca.dic", "orca-chars.dic", "orca-math.dic")
ESCAPES = {"t": "\t", "n": "\n", "r": "\r", "f": "\f", "v": "\v"}


def symbol_lines(data):
    """Yield original lines and decoded simple-symbol identifiers.

    Match Speech Dispatcher's single leading-backslash unescaping, rather
    than interpreting identifiers as Python escapes or regular expressions.
    """
    section = None
    for index, line in enumerate(data.splitlines(keepends=True)):
        text = line.decode("utf-8")
        if index == 0:
            text = text.removeprefix("\ufeff")
        text = text.rstrip("\r\n")
        key = None
        if text in ("symbols:", "complexSymbols:"):
            section = text
        elif section == "symbols:" and text and not text.startswith("#") and "\t" in text:
            key = text.split("\t", 1)[0]
            if key.startswith("\\") and len(key) > 1:
                key = key[1:]
                key = ESCAPES.get(key[0], key[0]) + key[1:]
        yield line, key


def replace_file(path, data):
    """Write completely before replacing a dictionary, preserving its mode."""
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as output:
            temporary = Path(output.name)
            output.write(data)
        temporary.chmod(stat.S_IMODE(path.stat().st_mode))
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def update_dictionaries(directory):
    """Remove only symbols covered by the same locale's NVDA dictionary."""
    for locale in sorted(directory.iterdir()):
        if not locale.is_dir():
            continue
        source = locale / "symbols.dic"
        if not source.is_file():
            continue
        covered = {key for _, key in symbol_lines(source.read_bytes()) if key is not None}
        if not covered:
            continue
        for name in ORCA_DICTIONARIES:
            path = locale / name
            if not path.is_file():
                continue
            original = path.read_bytes()
            lines = list(symbol_lines(original))
            retained = [(line, key) for line, key in lines if key is None or key not in covered]
            removed = len(lines) - len(retained)
            if not removed:
                continue
            if not any(key is not None for _, key in retained):
                path.unlink()
                print(f"Removed {path}: no symbols remain. "
                      "Remove its entry from nobase_localedata_DATA in Makefile.am.")
            else:
                replace_file(path, b"".join(line for line, _ in retained))
                print(f"Updated {path}: removed {removed} symbols covered by {source}.")


if __name__ == "__main__":
    update_dictionaries(Path(__file__).resolve().parent)
