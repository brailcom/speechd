Basic Tutorial
==============

This is the same as the quick start but with more explanation on everything. In this Tutorial we will create a speechd module which plays a audio file when ever it is asked to speak something.

Create a venv.
--------------

.. tabs::

   .. group-tab:: Local

    .. code-block:: bash

        mkdir -p ~/.local/libexec/speech-dispatcher-modules/
        cd ~/.local/libexec/speech-dispatcher-modules/
        python3.12 -m venv venv-dummy
        source venv-dummy/bin/activate

   .. group-tab:: System-Wide

    .. code-block:: bash

        mkdir -p /usr/libexec/speech-dispatcher-modules
        cd /usr/libexec/speech-dispatcher-modules
        python3.12 -m venv venv-dummy
        source venv-dummy/bin/activate

.. note:: In this case we are using python 3.12, but other versions will also likely work. But as of writing this, 3.14 is somewhat painful to use due to having to build many packages.

Install dependencies.
---------------------

.. code-block:: bash

    pip install --upgrade pip
    pip install pySpeechModule soundfile

Create your module.
-------------------

Now lets create your module. We are going to walk through each step here, but if you just want the full code the jump to the :ref:`full-code`

Add a shebang at the top of your python file.

.. tabs::

   .. group-tab:: Local

    .. note::

        When creating a module you should place it into ``~/.local/libexec/speech-dispatcher-modules`` for local installs. The name of the module must start with ``sd_``

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py0

        #!/home/user/.local/libexec/speech-dispatcher-modules/venv-dummy/bin/python

    .. attention::

        In our case we are using ``/home/user/`` as our home directory, yours will be different and should be changed to match.

        The last part ``venv/bin/python`` points to your venv's python binary. When python starts it automatically sets up your venv if you use this binary. This is exactly what we want to do.

   .. group-tab:: System-Wide

    .. note::

        When creating a module you should place it into ``/usr/libexec/speech-dispatcher-modules`` for system installs. The name of the module must start with ``sd_``

    .. code-block:: python3
        :caption: /usr/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py0

        #!/usr/libexec/speech-dispatcher-modules/venv-dummy/bin/python

    .. attention::

        The last part ``venv/bin/python`` points to your venv's python binary. When python starts it automatically sets up your venv if you use this binary. This is exactly what we want to do.

Now add your imports.

.. tabs::

   .. group-tab:: Local

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py1

        import logging
        import soundfile as sf
        from pySpeechModule import SpeechServer, SpeechDispatch

   .. group-tab:: System-Wide

    .. code-block:: python3
        :caption: /usr/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py1

        import logging
        import soundfile as sf
        from pySpeechModule import SpeechServer, SpeechDispatch

Here we are using pySpeechModule, soundfile and standard python logging.

Now lets create a class.

.. tabs::

   .. group-tab:: Local

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py2

        class Dummy(SpeechDispatch):

   .. group-tab:: System-Wide

    .. code-block:: python3
        :caption: /usr/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py2

        class Dummy(SpeechDispatch):

.. warning:: You can name your class anything, but make sure your class inherits from ``SpeechDispatch``.

Now `download <https://github.com/brailcom/speechd/raw/refs/heads/master/doc/pySpeechModule/source/deep_learning.wav>`_ and save the file as ``deep_learning.wav``. Then add a ``__init__`` method to your class.

.. tabs::

   .. group-tab:: Local

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py3

        def __init__(self):
            logging.info("Dummy Started!")
            self.file_path = "deep_learning.wav"

   .. group-tab:: System-Wide

    .. code-block:: python3
        :caption: /usr/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py3

        def __init__(self):
            logging.info("Dummy Started!")
            self.file_path = "deep_learning.wav"

.. note:: ``__init__`` is the were you will want to place anything that should be long lived. Once your class is initialized and passed to the server it will live for the life of the module. In this simple example the only thing we will place here is ``self.file_path``

Now lets create the ``speak`` method. This method will get called by the server when ever we get a request to speak text. It will then yield back data with the audio. The yield will return a dict with keys for ``audio`` for audio data, ``rate`` for the audio's sample rate, and ``mark`` for the text index mark. We will explain the mark in a later example, for now it can be set to a empty string.

.. tabs::

   .. group-tab:: Local

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py4

        def speak(self,text):
            try:
                data, samplerate = sf.read(self.file_path, dtype='int16')
                yield {"audio": data, "rate": samplerate, "mark": ""}
            except Exception as e:
                logging.error("An error occurred:", e)

   .. group-tab:: System-Wide

    .. code-block:: python3
        :caption: /usr/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py4

        def speak(self,text):
            try:
                data, samplerate = sf.read(self.file_path, dtype='int16')
                yield {"audio": data, "rate": samplerate, "mark": ""}
            except Exception as e:
                logging.error("An error occurred:", e)

.. warning:: when creating audio you must output it in ``dtype='int16'`` in this simple example we can just read into that format. In the next example we will show you how to convert audio to get that format.

Last we need to create the server and create a instance of our class.

.. tabs::

   .. group-tab:: Local

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py5

        SpeechServer().start(Dummy)

   .. group-tab:: System-Wide

    .. code-block:: python3
        :caption: /usr/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py5

        SpeechServer().start(Dummy)

.. _full-code:

Full Code.
----------

Here is the complete code. You will also need to `download <https://github.com/brailcom/speechd/raw/refs/heads/master/doc/pySpeechModule/source/deep_learning.wav>`_ if you have not already and make sure to name it ``deep_learning.wav``.

.. tabs::

   .. group-tab:: Local

    .. code-block:: python3
        :caption: /home/user/.local/libexec/speech-dispatcher-modules/sd_dummy.py
        :name: dummy-py

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
        :name: dummy-py

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

Add the conf file.
------------------

.. tabs::

   .. group-tab:: Local

    .. code-block:: bash

        touch ~/.config/speech-dispatcher/modules/dummy.py.conf

   .. group-tab:: System-Wide

    .. code-block:: bash

        touch /etc/speech-dispatcher/modules/dummy.py.conf


Testing the module.
-------------------

First stop any running speech-dispatch processes.

.. code-block:: bash

    pkill -9 speech-dispatch

Then send a speak command to speechd using ``spd-say``.

.. code-block:: bash

    spd-say -o "dummy.py" -y "en" "Hello world."

While this is pretty cool its not very useful to just play the same audio file for all speak commands. In the next tutorial we will create a module that actually speaks using Kokoro as our tts model.