Speech Dispatcher in Detail
===========================

Key features:
-------------

  * Common interface to different TTS engines
  * Handling concurrent synthesis requests — requests may come assynchronously
  from multiple sources within an application and/or from different applications
  * Subsequent serialization, resolution of conflicts and priorities of
  incomming requests
  * Context switching — state is maintained for each client connection
  independently, event for connections from within one application
  * High-level client interfaces for popular programming languages
  * Common sound output handling — audio playback is handled by Speech
  Dispatcher rather than the TTS engine, since most engines have limited sound
  output capabilities

What is a very high level GUI library to graphics, Speech Dispatcher is to
speech synthesis. The application neither needs to talk to the devices directly
nor to handle concurrent access, sound output and other tricky aspects of the
speech subsystem.

Supported TTS engines:
----------------------

  * Festival
  * Flite
  * Espeak
  * Cicero
  * IBM TTS
  * Espeak+MBROLA (through a generic driver)
  * Epos (through a generic driver)
  * DecTalk software (through a generic driver)
  * Cepstral Swift (through a generic driver)
  * Ivona
  * Pico

Supported sound output subsystems:
----------------------------------

  * OSS
  * ALSA
  * PulseAudio
  * NAS

The architecture is based on a client/server model. The clients are all the
applications in the system that want to produce speech (typically assistive
technologies). The basic means of client communication with Speech Dispatcher
is through a Unix socket or Inet TCP connection using the Speech Synthesis
Interface Protocol (See the SSIP documentation for more information). High-level
client libraries for many popular programming languages implement this protocol
to make its usage as simple as possible.

Supported client interfaces:
----------------------------

  * C/C++ API
  * Python 3 API
  * Java API
  * Emacs Lisp API
  * Common Lisp API
  * Guile API
  * Simple command line client

Existing assistive technologies known to work with Speech Dispatcher:

  * speechd-el (see https://devel.freebsoft.org/speechd-el)
  * Orca (see http://live.gnome.org/Orca/SpeechDispatcher)
  * Yasr (see http://yasr.sourceforge.net/)
  * BrlTTY (see http://brltty.com)
