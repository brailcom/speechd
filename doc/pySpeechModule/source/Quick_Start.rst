Quick Start.
============

If you have not already see the Basic_Tutorial for a more detailed explaintion of whats going on.

.. code-block:: bash

    mkdir my_first_speechd_module
    cd my_first_speechd_module
    python3.12 -m venv venv
    source venv/bin/activate
    pip install --upgrade pip
    pip install pySpeechModule soundfile

You need to `download <https://github.com/jsett/pySpeechModule/raw/refs/heads/main/docs/source/deep_learning.wav>`_ and make sure to name it ``deep_learning.wav``.

.. code-block:: python3
    :caption: dummy.py
    :name: dummy-py-quick

    #!/home/user/src/my_first_speechd_module/venv/bin/python

    import logging
    import soundfile as sf
    from pySpeechModule import SpeechServer, SpeechDispatch

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

    SpeechServer().start(Dummy())

Now you need to make the file executable

.. code-block:: bash

    chmod +x dummy.py

Finally add a configure line to speechd's configure file. speechd's configure file is usually found in ``~/.config/speech-dispatcher/speechd.conf``, but may be in a different location depending on your distro.

.. code-block:: bash
    :caption: speechd.conf
    :name: speechd-conf-quick

    Timeout 0
    AddModule "dummypy" "/home/user/src/my_first_speechd_module/dummy.py" "dummypy.conf"

To test the module. First stop any running speech-dispatch processes.

.. code-block:: bash

    pkill -9 speech-dispatch

Then send a speak command to speechd using ``spd-say``.

.. code-block:: bash

    spd-say -o "dummypy" -y "en" "Hello world."