# MIT License

# Copyright (c) 2026 John Settlemyer

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import logging
import time

class SpeechDispatch():
    def __init__(self):
        """
        you should place the voices list here in __init__

        .. code-block:: python

            self.voices = [{"name": "Testing", "language": "en", "variant": "none"},
            {"name": "Testing2", "language": "en", "variant": "none"}]

        You should also set your max_ahead and min_ahead here.

        .. code-block:: python

            self.max_ahead = 60 # this sets the max number of second your generation will get ahead of played audio.
            self.min_ahead = 30 # this sets how small the ahead number maybe before starting generation again.

        You can also place model init or what ever else you would like to init here.
        """
        pass
    def _set_init(self,queue, server):
        """
        Set the action_queue and server.

        :meta private:
        """

        self.action_queue = queue
        self._server = server
    def configure(self, configure_file: str):
        """
        This method get passed in the file path to the modules dot conf file
        If you need to use it you should open the file and parse here.
        """
        pass
    def change_voice(self, voice: str):
        """
        This method gets passed the name of a voice when the user has requested to change the voice.

        :param voice: String containing the voice name.
        :type String: String
        """
        pass
    def change_language(self, language: str):
        """
        This method gets passed a language when the user has requested to change the language.

        :param voice: String containing the language.
        :type String: String
        """
        pass
    def change_speed(self, speed: str):
        """
        This method gets passed a speed when the user requests to change the speed.
        The speed will be between -100 and 100

        :param voice: String containing the new speed.
        :type String: String
        """
        pass
    def change_pitch(self, pitch: str):
        """
        This method gets passed a pitch when the user requests to change the pitch

        :param voice: String containing the new pitch.
        :type String: String
        """
        pass
    def _parse_settings(self,options: dict):
        """
        When passed the settings it calls the convenience methods associated with that setting.

        :meta private:
        """
        if 'synthesis_voice' in options:
            self.change_voice(options['synthesis_voice'])
        if 'language' in options:
            self.change_language(options['language'])
        if 'rate' in options:
            self.change_speed(options['rate'])
        if 'pitch' in options:
            self.change_pitch(options['pitch'])
    def _settings(self,options):
        """
        Passed the setting, sends it to the parsers.

        :meta private:
        """
        self._parse_settings(options)
        self.settings(options)

    def settings(self,options: dict):
        """
        This gets passed the full settings options

        :param options: Dict containing all the settings options that were sent.
        :type Dictionary: Dictionary.
        """
        pass
    def _speak(self,text: str):
        """
        This is called to generate speech
        It then iterate's through speech chunks returned by the user.

        :meta private:
        """
        self._server.stop_set(False)
        self._ahead = 0
        self.action_queue.put({"command": "begin"})
        it = self.speak(text)
        while (True):
            try:
                if (self._server.stop_get()):
                    break
                start_time = time.time()
                item = next(it) # get the next audio chunk
                end_time = time.time()
                total_time = end_time-start_time # total time it took to generate that audio chunk
                produced_time = (len(item['audio']) / item['rate']) # amount of audio generate
                logging.debug(f"Speak chunk took {total_time} to produce {produced_time} for mark {item['mark']}")
                self._ahead = self._ahead + produced_time
                self._ahead = self._ahead - total_time
                logging.debug(f"Ahead: {self._ahead}")
                self.action_queue.put({"command": "speak", "val": item})
                # sleep if we have too much audio generated, only do this is max_ahead and min_ahead are set.
                if (hasattr(self, "max_ahead") and hasattr(self, "min_ahead")):
                    if (self._ahead >= self.max_ahead):
                        logging.debug(f"Ahead by too much sleeping")
                        while (self._ahead >= self.min_ahead):
                            time.sleep(1)
                            self._ahead = self._ahead - 1
                            if (self._server.stop_get()):
                                break;
            except StopIteration:
                logging.debug("finished speak chunking")
                break
        self.action_queue.put({"command": "end"})
    def speak(self,text: str):
        """
        This method get called to get the user to generate audio.
        Data must be yield'ed in the format

        .. code-block:: python

            yield {"audio": <data>, "rate": <samplerate>, "mark": <mark>}

        :param text: String containing ssml text.
        :type String: String
        :yields: Dict containing keys for "audio", "rate", "mark"
        :ytype: Dict
        """
        pass