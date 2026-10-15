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
import threading
import queue

class ActionWorker(threading.Thread):
    """
    Worker for running speak commands on the server

    :meta private:
    """
    def __init__(self, server):
        super().__init__()
        self.queue = queue.Queue()
        self.server = server
        self.daemon = True

    def begin(self):
        self.server._begin()

    def end(self):
        self.server._end()

    def speak(self,val):
        self.server._send_audio(val)

    def stop(self):
        """Sends the sentinel value to trigger a graceful shutdown."""
        self.queue.put(None)

    def run(self):
        while True:
            item = self.queue.get()
            logging.debug(f"action worker: got item {item}")
            if item is None:
                break
            if (item['command'] == 'begin'):
                self.begin()
            if (item['command'] == 'end'):
                self.end()
            if (item['command'] == 'speak'):
                self.speak(item['val'])