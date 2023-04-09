# Copyright (C) 2001, 2002 Brailcom, o.p.s.
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

"""Python API to Speech Dispatcher

This is the Python client API to Speech Dispatcher.

Two main classes are provided:

- speechd.client.SSIPClient : direct mapping of the SSIP commands and logic
- speechd.client.Speaker : a more convenient interface.

You can use

pydoc3 speechd.client.SSIPClient
pydoc3 speechd.client.Speaker

to get their documentation.
"""

from .client import *

