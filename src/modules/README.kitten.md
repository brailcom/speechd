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

# Downloading models+voices.

A simple downloader for downloading the models and voices is provided [here](https://github.com/jsett/kitten_models_downloader)
