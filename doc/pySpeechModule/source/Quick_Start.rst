Quick Start.
============

If you have not already see the :doc:`Basic Tutorial <Basic_Tutorial>` for a more detailed explaintion of whats going on.

.. tabs::

   .. group-tab:: Local

    .. code-block:: bash

        mkdir -p ~/.local/libexec/speech-dispatcher-modules/
        cd ~/.local/libexec/speech-dispatcher-modules/
        python3.12 -m venv venv-dummy
        source venv-dummy/bin/activate
        pip install --upgrade pip
        pip install pySpeechModule soundfile

   .. group-tab:: System-Wide

    .. code-block:: bash

        mkdir -p /usr/libexec/speech-dispatcher-modules
        cd /usr/libexec/speech-dispatcher-modules
        python3.12 -m venv venv-dummy
        source venv-dummy/bin/activate
        pip install --upgrade pip
        pip install pySpeechModule soundfile


You need to `download <https://github.com/brailcom/speechd/raw/refs/heads/master/doc/pySpeechModule/source/deep_learning.wav>`_ and make sure to name it ``deep_learning.wav``.

.. tabs::

   .. group-tab:: Local

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py-quick

        #!/home/user/.local/libexec/speech-dispatcher-modules/venv-dummy/bin/python

        import logging
        import soundfile as sf
        from speechd_module import SpeechServer, SpeechDispatch

        class Dummy(SpeechDispatch):
            def __init__(self):
                logging.info("Dummy Started!")
                self.file_path = "deep_learning.wav"
            def speak(self,text):
                try:
                    data, samplerate = sf.read(self.file_path, dtype='int16')
                    yield {"audio": data, "rate": samplerate, "mark": ""}
                except Exception as e:
                    logging.error("An error occurred:", e)

        SpeechServer().start(Dummy)

   .. group-tab:: System-Wide

    .. code-block:: python3
        :caption: /usr/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py-quick

        #!/usr/libexec/speech-dispatcher-modules/venv-dummy/bin/python

        import logging
        import soundfile as sf
        from speechd_module import SpeechServer, SpeechDispatch

        class Dummy(SpeechDispatch):
            def __init__(self):
                logging.info("Dummy Started!")
                self.file_path = "deep_learning.wav"
            def speak(self,text):
                try:
                    data, samplerate = sf.read(self.file_path, dtype='int16')
                    yield {"audio": data, "rate": samplerate, "mark": ""}
                except Exception as e:
                    logging.error("An error occurred:", e)

        SpeechServer().start(Dummy)

Now you need to make the file executable

.. code-block:: bash

    chmod +x sd_dummy.py

Add the .conf file.

.. tabs::

   .. group-tab:: Local

    .. code-block:: bash

        touch ~/.config/speech-dispatcher/modules/dummy.py.conf

   .. group-tab:: System-Wide

    .. code-block:: bash

        touch /etc/speech-dispatcher/modules/dummy.py.conf


To test the module. First stop any running speech-dispatch processes.

.. code-block:: bash

    pkill -9 speech-dispatch

Then send a speak command to speechd using ``spd-say``.

.. code-block:: bash

    spd-say -o "dummy.py" -y "en" "Hello world."