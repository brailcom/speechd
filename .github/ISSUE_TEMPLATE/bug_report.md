---
name: Bug report
about: Create a report to help us improve speech-dispatcher
title: ''
labels: ''
assignees: ''

---

### Steps to reproduce

Please describe how you make the issue happen, so we can reproduce it.

### Obtained behavior

Please describe the result of your actions, and notably what you got that you didn't expect

### Expected behavior

Please describe the result that you expected instead.

### Behavior information

Please follow the next step, to provide us with precious information about how things went wrong on your machine:

* Set `LogLevel` to 5 in your `/etc/speech-dispacher/speechd.conf`
* Restart your computer to make sure it gets taken into account
* Reproduce the issue
* Make a copy of the `speech-dispacher.log` file, it is usually found in `/run/user/1000/speech-dispatcher/log`. Also make a copy of `yourspeechmodule.log` (e.g. `espeak-ng.log`).
* Set `LogLevel` back to 3 in your `/etc/speech-dispacher/speechd.conf`
* Attach the `speech-dispatcher.log` and `yourspeechmodule.log` file to your bug report.

### Distribution



### Version of Speech-dispatcher
