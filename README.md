speech-dispatcher
=================

*Common interface to speech synthesis*

Introduction
------------

This is the Speech Dispatcher project (speech-dispatcher). It is a part of the
[Free(b)soft project](https://devel.freebsoft.org/), which is intended to allow
blind and visually impaired people to work with computer and Internet based on
free software.

Speech Dispatcher project provides a high-level *device independent* layer
for access to speech synthesis through a simple, stable and well documented
interface.

Documentation
-------------

Complete documentation may be found in doc directory:
the speech dispatcher documentation: doc/speech-dispatcher.html,
the spd-say documentation: doc/spd-say.html,
and the SSIP protocol documentation: doc/ssip.html.

Read [doc/README](doc/README) for more information.

This documentation is also available online:
the [speech dispatcher documentation](http://htmlpreview.github.io/?https://github.com/brailcom/speechd/blob/master/doc/speech-dispatcher.html),
the [spd-say documentation](http://htmlpreview.github.io/?https://github.com/brailcom/speechd/blob/master/doc/spd-say.html),
and the [SSIP protocol documentation](http://htmlpreview.github.io/?https://github.com/brailcom/speechd/blob/master/doc/ssip.html).

The key features and the supported TTS engines, output subsystems, client
interfaces and client applications known to work with Speech Dispatcher are
listed in [overview of speech-dispatcher](README.overview.md) as well as voices
settings and where to look at in case of a sound or speech issue.

Mailing-lists
-------------

There is a public mailing-list
[speechd-discuss](https://lists.nongnu.org/mailman/listinfo/speechd-discuss)
for this project.

This list is for Speech Dispatcher developers, as well as for users. If you
want to contribute the development, propose a new feature, get help or just be
informed about the latest news, don't hesitate to subscribe. The communication
on this list is held in English.

Development
-----------

Various versions of speech-dispatcher can be downloaded from the [project
archive](https://github.com/brailcom/speechd/releases).

Bug reports, issues, and patches can be submitted to [the github
tracker](https://github.com/brailcom/speechd/issues).

The source code is freely available. It is managed using Git. You can use
the [GitHub web interface](https://github.com/brailcom/speechd) or clone the
repository from:

    https://github.com/brailcom/speechd.git


Modules for different speech synthesis backends can easily be developped in
different ways. This allows to integrate all kinds of speech syntheses, be
they C libraries, external commands, or even http services, and whatever their
licences since the interface between the speechd server and the syntheses is a
mere pipe between processes with a very simple protocol. More details are
available in the
[Output Modules documentation](https://htmlpreview.github.io/?https://github.com/brailcom/speechd/blob/master/doc/speech-dispatcher.html#Output-Modules)


Rust bindings are currently developed separately. You can use the [GitLab web
interface](https://gitlab.com/ndarilek/speech-dispatcher-rs) or clone the
repository from:

    https://gitlab.com/ndarilek/speech-dispatcher-rs.git

A Java library is currently developed separately. You can use the [GitHub web
interface](https://github.com/brailcom/speechd-java) or clone the repository
from:

    https://github.com/brailcom/speechd-java.git

To build and install speech-dispatcher and all of it's components, read the
file [INSTALL](INSTALL).


To try the just-built speech-dispatcher without installing it (e.g. to avoid
disturbing an existing speech-dispatcher instance), you can run

```
./run-speechd -t 0 -s
```

and in another terminal run

```
./run-spd-say foo
```

You can also make other speech-dispatcher clients connect to your dedicated
instance by first running

```
export SPEECHD_ADDRESS=inet_socket:127.0.0.1:6561
```

If you want those clients to also use the just-built libspeechd, you can
additionally use

```
export LD_LIBRARY_PATH=$PWD/src/api/c/.libs
```


People
------

Speech Dispatcher is being developed in closed cooperation between the Brailcom
company and external developers, both are equally important parts of the
development team. The development team also accepts and processes contributions
from other developers, for which we are always very thankful! See more details
about our development model in Cooperation. Bellow find a list of current inner
development team members and people who have contributed to Speech Dispatcher in
the past:

Development team:

  * Samuel Thibault
  * Jan Buchal
  * Tomas Cerha
  * Hynek Hanke
  * Milan Zamazal
  * Luke Yelavich
  * C.M. Brannon
  * William Hubbs
  * Andrei Kholodnyi

Contributors: Trevor Saunders, Lukas Loehrer,Gary Cramblitt, Olivier Bert, Jacob
Schmude, Steve Holmes, Gilles Casse, Rui Batista, Marco Skambraks ...and many
others.

Licensing
---------

Speech Dispatcher uses several layers of software, to allow for flexible
licensing.

The central speechd server is essentially GPLv2.

The C api client library is essentially LGPL2.1-or-later, which thus allows to
use it in various applications with little licensing concerns. It is connected
to the central server through a socket with the SSIP protocol, in such a way
that GPL licensing propagation doesn't apply.

The speech modules are connected to the central server through a pipe with
a very simple protocol similar to SSIP, in such a way that GPL licensing
propagation doesn't apply either.

To make writing speech modules simpler, a libspeechd_module library is provided
under a BSD-2 license, which can thus be combined with essentially any other
license.

Some more advanced module helpers are also provided under LGPLv2.1-or-later, but
they are not mandatory.



In detail:

- The speech-dispatcher server (src/server/ + src/common/) contains
GPLv2-or-later and LGPLv2.1-or-later source code, but is linked against
libdotconf, which is LGPLv2.1-only at the time of writing.

- The speech-dispatcher modules (src/modules/ + src/common/ + src/audio/)
contain GPLv2-or-later, LGPLv2.1-or-later, LGPLv2-or-later, and BSD-2 source
code, but some parts are also linked against libdotconf, which is LGPLv2.1-only
at the time of writing.

- The spd-conf tool (src/api/python/speechd_config/), spd-say tool
(src/clients/say), and spdsend tool (src/clients/spdsend/) are GPLv2-or-later.

- The C API library (src/api/c/) is LGPLv2.1-or-later

- The Common Lisp API library (src/api/cl/) is LGPLv2.1-or-later.

- The Guile API library (src/api/guile/) contains GPLv2-or-later and
LGPLv2.1-or-later source code.

- The Python API library (src/api/python/speechd/) is LGPLv2.1-or-later.

- All tests in src/tests/ are GPLv2-or-later.



Copyright (C) 2001-2009 Brailcom, o.p.s
Copyright (C) 2018-2020, 2022, 2024-2025 Samuel Thibault <samuel.thibault@ens-lyon.org>
Copyright (C) 2018 Didier Spaier <didier@slint.fr>

This README file is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This README file is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details (file
COPYING in the root directory).

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
