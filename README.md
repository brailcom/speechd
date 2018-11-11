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

Complete documentation may be found in doc directory. Read
[doc/README](doc/README) for more information. This [documentation is also
available online](https://devel.freebsoft.org/doc/speechd/speech-dispatcher.html).

The [SSIP communication protocol is also
documented](https://devel.freebsoft.org/doc/speechd/ssip.html).

The key features and the supported TTS engines, output subsystems, client
interfaces and client applications known to work with Speech Dispatcher are
listed in [overview of speech-dispatcher](README.overview.md) as well as voices
settings and where to look at in case of a sound or speech issue.

Mailing-lists
-------------

There is a public mailing-list speechd@lists.freebsoft.org for this project.

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

A Java library is currently developed separately. You can use the [GitHub web
interface](https://github.com/brailcom/speechd-java) or clone the repository
from:

    https://github.com/brailcom/speechd-java.git

To build and install speech-dispatcher and all of it's components, read the
file [INSTALL](INSTALL).


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

License
-------

Copyright (C) 2001-2009 Brailcom, o.p.s
Copyright (C) 2018 Samuel Thibault <samuel.thibault@ens-lyon.org>
Copyright (C) 2018 Didier Spaier <didier@slint.fr>

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details (file
COPYING in the root directory).

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 675 Mass
Ave, Cambridge, MA 02139, USA.


NOTE: speech dispatcher currently depends on dotconf, which is LGPLv2.1-only at
the time of writing.
