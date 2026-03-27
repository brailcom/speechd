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

Support
-------

These speech syntheses are supported:

- Baratinoo / VoxyGen
- Cicero
- DECTalk
- Epos
- ESpeak/ESpeak-NG
- Festival
- Flite
- IBMTTS / voxin
- Ivona
- Kali
- Llia
- Mary
- MBrola
- Mimic3
- OpenJTalk
- Pico
- Piper
- Swift

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

The python binding documentation is available on the shell with
`pydoc3 speechd` (or `pydoc speechd`)
and online: the [speechd.client module documentation](http://htmlpreview.github.io/?https://github.com/brailcom/speechd/blob/master/doc/speechd.client.html)

The key features and the supported TTS engines, output subsystems, client
interfaces and client applications known to work with Speech Dispatcher are
listed in [overview of speech-dispatcher](README.overview.md) as well as voices
settings and where to look at in case of a sound or speech issue.

Installation
------------

To build and install Speech Dispatcher from source, use the standard autotools sequence:

    ./build.sh
    ./configure
    make
    sudo make install

For detailed build requirements and dependencies, please refer to the [INSTALL](INSTALL) file.

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


Modules for differ