Advanced Tutorial.
==================

In this tutorial we will create a speechd module that can generate tts output, is able to change speed and voices. We will be using `Kokoro <https://huggingface.co/hexgrad/Kokoro-82M>`_ a open-weight TTS model with 82 million parameters to generated audio for us.

Setup the directories and venv.
-------------------------------

.. code-block:: bash

    mkdir kokoro_speechd_module
    cd kokoro_speechd_module

    python3.12 -m venv venv
    source venv/bin/activate

.. note:: In this case we are using python 3.12. As of writing this, 3.14 is somewhat painful to use due to having to build many packages, I would recommend 3.12 for now.

Install dependencies.
---------------------

.. code-block:: bash

    pip install --upgrade pip
    pip install pySpeechModule "kokoro>=0.9.4" soundfile

Verify that kokoro is working.
------------------------------

Start python and run the following. You should get a .wav file back that says "hello world".

.. code-block:: python3

    from kokoro import KPipeline
    import soundfile as sf
    import torch
    pipeline = KPipeline(lang_code='a')
    text = "Hello world."
    generator = pipeline(text, voice='af_heart')
    for i, (gs, ps, audio) in enumerate(generator):
        print(i, gs, ps)
        sf.write(f'{i}.wav', audio, 24000)


Create your module.
-------------------

.. code-block:: python3
    :caption: kokoro_module.py
    :name: kokoro-py

    #!/home/user/src/kokoro_speechd_module/venv/bin/python

    import soundfile as sf
    from kokoro import KPipeline
    import logging
    import torch
    from pySpeechModule import SpeechServer, SpeechDispatch, parse_ssml
    import io

    class KokoroDispatch(SpeechDispatch):
        def __init__(self):
            self.pipeline = KPipeline(lang_code='a')
            self.speed = 1.0
            self.voice = 'af_heart'
            self.voices = [{"name": "af_heart", "language": "en", "variant": "none", "kokoro_lang": 'a'},
            {"name": "af_alloy", "language": "en", "variant": "none", "kokoro_lang": 'a'}]
            self.max_ahead = 60
            self.min_ahead = 30

        def change_speed(self, speed):
            # callback for changing the speed.
            # speechd gives speed between -100 and 100
            # we have to convert that into a multiplier
            speed = int(speed)
            if speed < 0:
                self.speed = 1.0 + (float(speed) / 100.0) * 0.5
            else:
                self.speed = 1.0 + (float(speed) / 100.0) * 2.0

        def change_voice(self, voice_name):
            # call back for changing the voice.
            logging.critical(f"Changed voice_name: {voice_name}")
            try:
                filtered = [voice for voice in self.voices if voice["name"] == voice_name][0]
            except:
                filtered = self.voices[0]
            self.voice = filtered["name"]
            self.pipeline = KPipeline(lang_code=filtered["kokoro_lang"])

        def speak(self, ssml):
            try:
                # parse the ssml into chunk of text each with a mark.
                for item in parse_ssml(ssml):
                    text = item['text']
                    mark = item['mark']
                    # setup Kokoro's generator
                    generator = self.pipeline(text, voice=self.voice, speed=self.speed)
                    # iterate over Kokoro's generator.
                    for i, (gs, ps, audio) in enumerate(generator):
                        # use sound file to convert the output audio to a format we can use
                        buff = io.BytesIO() # create a in memory buffer
                        buff.name = "tmp.wav"
                        samplerate = 24000
                        sf.write(buff, audio, samplerate, subtype="PCM_16") # write the current audio to the buffer
                        # you can use sf.available_subtypes() to get a list available subtypes, and match your models input audio format.
                        buff.seek(0) # set the buffer back to start
                        audio_data, output_sample_rate = sf.read(buff, dtype="int16") # read the buffer in the correct format.
                        yield {"audio": audio_data, "rate": output_sample_rate, "mark": mark} # you must yield data even if you have a single output.
            except Exception as e:
                print("An error occurred:", e)

    SpeechServer().start(KokoroDispatch())

This should like somewhat similar to the previous example. But lets go through the methods and explain each of them.

__init__()
----------

.. code-block:: python3

    def __init__(self):
        self.pipeline = KPipeline(lang_code='a')
        self.speed = 1.0
        self.voice = 'af_heart'
        self.voices = [{"name": "af_heart", "language": "en", "variant": "none", "kokoro_lang": 'a'},
        {"name": "af_alloy", "language": "en", "variant": "none", "kokoro_lang": 'a'}]
        self.max_ahead = 60
        self.min_ahead = 30

Here we are creating ``pipeline`` which is the core class for our Kokoro pipeline. In addition to that we create a few other attributes. You should pay close attention to ``self.voices``, ``self.max_ahead`` and ``self.min_ahead``. These are special attributes used by the module. ``self.voices`` is used to generated the list of voices you would get when a client asks for a list of voices(for example ``spd-say -L``). ``max_ahead`` and ``min_ahead`` define the max and min number of seconds the generated audio can get ahead of your played audio. This saves your cpu from running too hard and is automatically handled in the background if you provide these values. A good default is to use 60 and 30.

Change speed.
-------------

.. code-block:: python3

    def change_speed(self, speed):
        # callback for changing the speed.
        # speechd gives speed between -100 and 100
        # we have to convert that into a multiplier
        speed = int(speed)
        if speed < 0:
            self.speed = 1.0 + (float(speed) / 100.0) * 0.5
        else:
            self.speed = 1.0 + (float(speed) / 100.0) * 2.0

This is a callback that gets called when the client asks to change the speed. There is not much to this since we are simply setting a variable. The only important thing to note is that speed will be between -100 and 100.

Change voice.
-------------

.. code-block:: python3

    def change_voice(self, voice_name):
        # call back for changing the voice.
        try:
            filtered = [voice for voice in sel.voices if voice["name"] == voice_name][0]
        except:
            filtered = self.voices[0]
        self.voice = filtered["name"]
        self.pipeline = KPipeline(lang_code=filtered["kokoro_lang"])

This is a callback that will get called when we are asked to change the voice being used. Here we also are updating the pipeline since the ``lang_code`` is being set in the pipeline and the voice and language are linked together. In this example we add a extra key to voices `kokoro_lang`. This is necessary because the language code for kokoros is different from the speechd language code. `kokoro_lang` is not a necessary key for the speechd module.

Speak.
------

.. code-block:: python3

    def speak(self, ssml):
        try:
            # parse the ssml into chunk of text each with a mark.
            for item in parse_ssml(ssml):
                text = item['text']
                mark = item['mark']
                # setup Kokoro's generator
                generator = self.pipeline(text, voice=self.voice, speed=self.speed)
                # iterate over Kokoro's generator.
                for i, (gs, ps, audio) in enumerate(generator):
                    # use sound file to convert the output audio to a format we can use
                    buff = io.BytesIO() # create a in memory buffer
                    buff.name = "tmp.wav"
                    samplerate = 24000
                    sf.write(buff, audio, samplerate, subtype="PCM_16") # write the current audio to the buffer
                    # you can use sf.available_subtypes() to get a list available subtypes, and match your models input audio format.
                    buff.seek(0) # set the buffer back to start
                    audio_data, output_sample_rate = sf.read(buff, dtype="int16") # read the buffer in the correct format.
                    yield {"audio": audio_data, "rate": output_sample_rate, "mark": mark} # you must yield data even if you have a single output.
        except Exception as e:
            print("An error occurred:", e)

We showed this callback in the previous tutorial. It is a callback which is called when we are asked to speak something. 

Lets now break it all this down.

.. code-block:: python3

    for item in parse_ssml(ssml):

``parse_ssml`` is a convenience function which you can use to turn the ssml(which is really just like xml) into a python list of dict's. Each dict has the text and the mark. The mark is a way of telling speechd where you are at in the text, you can think of it as a index. if you have a few sentence's of text and you speak up to the second sentence you would send the mark linked to the second sentence to tell speechd what has been spoken.

.. code-block:: python3

    generator = self.pipeline(text, voice=self.voice, speed=self.speed)
    # iterate over Kokoro's generator.
    for i, (gs, ps, audio) in enumerate(generator):

This is how kokoro generates audio. It also does it own chunking. Which is awesome since we have no problem sending chunked audio out. We only have on issue here kokoro generates audio in a format we can not use directly( but we can convert it).

.. code-block:: python3

    buff = io.BytesIO()
    buff.name = "tmp.wav"
    samplerate = 24000
    sf.write(buff, audio, samplerate, subtype="PCM_16")
    # you can use sf.available_subtypes() to get a list available subtypes, and match your models input audio format.
    buff.seek(0) # set the buffer back to start
    audio_data, output_sample_rate = sf.read(buff, dtype="int16") # read the buffer in the correct format.

This code converts the audio to a format we can use. The first part of this we are creating a in memory file, using pythons built-in ``BytesIO``. We then can use sound file to write our audio to that BytesIO object. We use ``subtype`` to specify what the input audio format was. Next we need to reset the position on our in memory file, we do this with a seek. Then finally we are able to read the file back out in a format we can use.

.. tip:: you can use ``sf.available_subtypes()`` to get a list available subtypes, and match your models input audio format.

.. code-block:: python3

    yield {"audio": audio_data, "rate": output_sample_rate, "mark": mark}

This last line is used to "return" the output of a chunk of audio along with the sample rate and mark. You can use a yield inside a loop to return multiple chunks of audio.

.. code-block:: python3

    SpeechServer().start(KokoroDispatch())

Finally we start the server and pass a instance of the class.

Setup your config.
------------------

When speechd run's you script it needs to file to be executable. You can use ``chmod`` to do this.

.. code-block:: python3

    chmod +x kokoro_module.py

Finally add a configure line to speechd's config file. speechd's configure file is usually found in ``~/.config/speech-dispatcher/speechd.conf``, but may be in a different location depending on your distro.

.. code-block:: bash
    :caption: speechd.conf
    :name: speechd-conf-adv

    Timeout 0
    AddModule "kokoropy" "/home/user/src/kokoro_speechd_module/kokoro_module.py" "kokoropy.conf"

.. warning:: You should change ``/home/user/src/kokoro_speechd_module/kokoro_module.py`` to match your path.

Testing the module.
-------------------

First stop any running speech-dispatch processes.

.. code-block:: bash

    pkill -9 speech-dispatch

Then send a speak command to speechd using ``spd-say``.

.. code-block:: bash

    spd-say -o "kokoropy" -y "af_heart" "Hello world."

You should hear it speaking.

Lets try a diffrent voice.

.. code-block:: bash

    spd-say -o "kokoropy" -y "af_alloy" "Hello world."

Lets make it speak faster.

.. code-block:: bash

    spd-say -o "kokoropy" -r 25 -y "af_heart" "Hello world."

Congratulations, you have now create a speechd module that can generate tts output, is able to change speed and voices
