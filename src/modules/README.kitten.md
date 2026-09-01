# Module conf file options.

The modules dot conf file can be used to configure the module using `AddVoiceFile` and `modelPath`. `AddVoiceFile` let you set models/voices in the case of updated models. `modelPath` let you set the modes/voices search path.

## AddVoiceFile

AddVoiceFile should be formated like such.
```
AddVoiceFile "voice_quality" "model filename" "model sha256" "voices filename" "voices sha256"
```

you can find an example config [here](https://gist.github.com/jsett/b82d19d3e2e538a29e5e79995b619a62)

## modelPath

modelPath should be formated like such.
```
modelPath "<your path>"
```

## Example configuration file

`kittentts.conf`

```
modelPath "/usr/share/kitten"

AddVoiceFile "Normal" "kitten_tts_micro_v0_8.onnx" "95481626fee1ba70ce683e69c534fc7cb38433c46ce42d3abbeafb4b9f1a4123" "voices_micro.bin" "12ad10f1fcce8a458b5cf79769b8edd4ba0e11e9fb6532fd192c3500b2b37a5d"
AddVoiceFile "High" "kitten_tts_mini_v0_8.onnx" "0f5bbae4fc4800c98dbc544a87ecfa79510de2fb8222db30d12e5bfe9177df91" "voices_mini.bin" "0e4965b46333db53ce09c73842623bf7055ea62c67f78803cb7ea9c16da6ac2b"
AddVoiceFile "Low" "kitten_tts_nano_v0_2.onnx" "42fa8809db319cd7c4c83b3c501e2313bf90edf610235291cad605e4adcb242d" "voices_nano.bin" "42a40a24a352a38657d6cb86ceee51bbc2b7780b29e04fb60bcdf959adccea01"
```

# Downloading models+voices.

A simple downloader for downloading the models and voices is provided [here](https://github.com/jsett/kitten_models_downloader)
