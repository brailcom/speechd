/*
  MIT License

Copyright (c) 2022 Michael Hansen, 2025 Derek L Davies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <sys/time.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <limits>
#include <filesystem>
#include <sstream>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <espeak-ng/speak_lib.h>
#include <onnxruntime_cxx_api.h>
#include <json.hpp>
#include <piper.hpp>
#include <utf8.h>
#include <wavfile.hpp>
#include <rubberband/RubberBandStretcher.h>

#include "spd_audio.h"
#include <speechd_types.h>
#include "module_utils.h"

#define MODULE_NAME     "Cxxpiper"
#define DBG_MODNAME     "Cxxpiper"
#define MODULE_VERSION  "0.0"
#define DEBUG_MODULE 5
DECLARE_DEBUG();
//}
namespace cxxpiper {
    using namespace std;
    using json = nlohmann::json;
    // rubberband stretcher.  "Realtime" mode, streaming (single pass, no "study"), clipping be damned.
    // Used for real time pitch, rate and volume adjustment.
    using RubberBand::RubberBandStretcher;
    using namespace piper;

#ifdef _CXXPIPER_VERSION
#define _STR(x) #x
#define STR(x) _STR(x)
    const std::string VERSION = STR(_CXXPIPER_VERSION);
#else
    const std::string VERSION = "";
#endif
    std::string getVersion() { return VERSION; }
    const std::string instanceName{"cxxpiper"};

    const float MIN_WAV_VALUE = -32766.0f;
    const float MAX_WAV_VALUE = 32767.0f;

    static const int bs = 1024;
    static float **cbuf;
    static piper::PiperConfig piperConfig;
    static char *cmdInp;
    static int stop_requested;

    static size_t cbCnt = 0; // DEBUG only
    static size_t cbTot = 0;
    static size_t prCnt = 0;
    static size_t prTot = 0;
    static size_t izCnt = 0;
    static size_t izTot = 0;
    static size_t ezCnt = 0;
    static size_t ezTot = 0;

    extern "C" {
	static int cxxpiper_free_cbuf();
	static void cxxpiper_free_voice_list();
    }

// Copied from piper distribution.
// True if the string is a single UTF-8 codepoint
    bool isSingleCodepoint(std::string s)
    {
	return utf8::distance(s.begin(), s.end()) == 1;
    }

// Copied from piper distribution.
// Get the first UTF-8 codepoint of a string
    Phoneme getCodepoint(std::string s)
    {
	utf8::iterator character_iter(s.begin(), s.begin(), s.end());
	return *character_iter;
    }

// Copied from piper distribution.
    void parsePhonemizeConfig(json &configRoot, PhonemizeConfig &phonemizeConfig)
    {
	// {
	//     "espeak": {
	//         "voice": "<language code>"
	//     },
	//     "phoneme_type": "<espeak or text>",
	//     "phoneme_map": {
	//         "<from phoneme>": ["<to phoneme 1>", "<to phoneme 2>", ...]
	//     },
	//     "phoneme_id_map": {
	//         "<phoneme>": [<id1>, <id2>, ...]
	//     }
	// }
	if (configRoot.contains("espeak")) {
	    auto espeakValue = configRoot["espeak"];
	    if (espeakValue.contains("voice")) {
		phonemizeConfig.eSpeak.voice = espeakValue["voice"].get<std::string>();
	    }
	}
	if (configRoot.contains("phoneme_type")) {
	    auto phonemeTypeStr = configRoot["phoneme_type"].get<std::string>();
	    if (phonemeTypeStr == "text") {
		phonemizeConfig.phonemeType = TextPhonemes;
	    }
	}
	// phoneme to [id] map
	// Maps phonemes to one or more phoneme ids (required).
	if (configRoot.contains("phoneme_id_map")) {
	    auto phonemeIdMapValue = configRoot["phoneme_id_map"];
	    for (auto &fromPhonemeItem : phonemeIdMapValue.items()) {
		std::string fromPhoneme = fromPhonemeItem.key();
		if (!isSingleCodepoint(fromPhoneme)) {
		    std::stringstream idsStr;
		    for (auto &toIdValue : fromPhonemeItem.value()) {
			PhonemeId toId = toIdValue.get<PhonemeId>();
			idsStr << toId << ",";
		    }
		    //error("\"{}\" is not a single codepoint (ids={})", fromPhoneme,
		    //idsStr.str());
		    throw std::runtime_error(
			"Phonemes must be one codepoint (phoneme id map)");
		}
		auto fromCodepoint = getCodepoint(fromPhoneme);
		for (auto &toIdValue : fromPhonemeItem.value()) {
		    PhonemeId toId = toIdValue.get<PhonemeId>();
		    phonemizeConfig.phonemeIdMap[fromCodepoint].push_back(toId);
		}
	    }
	}
	// phoneme to [phoneme] map
	// Maps phonemes to one or more other phonemes (not normally used).
	if (configRoot.contains("phoneme_map")) {
	    if (!phonemizeConfig.phonemeMap) {
		phonemizeConfig.phonemeMap.emplace();
	    }
	    auto phonemeMapValue = configRoot["phoneme_map"];
	    for (auto &fromPhonemeItem : phonemeMapValue.items()) {
		std::string fromPhoneme = fromPhonemeItem.key();
		if (!isSingleCodepoint(fromPhoneme)) {
		    //error("\"{}\" is not a single codepoint", fromPhoneme);
		    throw std::runtime_error(
			"Phonemes must be one codepoint (phoneme map)");
		}
		auto fromCodepoint = getCodepoint(fromPhoneme);
		for (auto &toPhonemeValue : fromPhonemeItem.value()) {
		    std::string toPhoneme = toPhonemeValue.get<std::string>();
		    if (!isSingleCodepoint(toPhoneme)) {
			throw std::runtime_error(
			    "Phonemes must be one codepoint (phoneme map)");
		    }
		    auto toCodepoint = getCodepoint(toPhoneme);
		    (*phonemizeConfig.phonemeMap)[fromCodepoint].push_back(toCodepoint);
		}
	    }
	}
    }

// Copied from piper distribution.
    void parseSynthesisConfig(json &configRoot, SynthesisConfig &synthesisConfig)
    {
	if (configRoot.contains("audio")) {
	    auto audioValue = configRoot["audio"];
	    if (audioValue.contains("sample_rate")) {
		// Default sample rate is 22050 Hz
		synthesisConfig.sampleRate = audioValue.value("sample_rate", 22050);
	    }
	}
	if (configRoot.contains("inference")) {
	    // Overrides default inference settings
	    auto inferenceValue = configRoot["inference"];
	    if (inferenceValue.contains("noise_scale")) {
		synthesisConfig.noiseScale = inferenceValue.value("noise_scale", 0.667f);
	    }
	    if (inferenceValue.contains("length_scale")) {
		synthesisConfig.lengthScale = inferenceValue.value("length_scale", 1.0f);
	    }
	    if (inferenceValue.contains("noise_w")) {
		synthesisConfig.noiseW = inferenceValue.value("noise_w", 0.8f);
	    }
	    if (inferenceValue.contains("phoneme_silence")) {
		// phoneme -> seconds of silence to add after
		synthesisConfig.phonemeSilenceSeconds.emplace();
		auto phonemeSilenceValue = inferenceValue["phoneme_silence"];
		for (auto &phonemeItem : phonemeSilenceValue.items()) {
		    std::string phonemeStr = phonemeItem.key();
		    if (!isSingleCodepoint(phonemeStr)) {
			//error("\"{}\" is not a single codepoint", phonemeStr);
			throw std::runtime_error(
			    "Phonemes must be one codepoint (phoneme silence)");
		    }
		    auto phoneme = getCodepoint(phonemeStr);
		    (*synthesisConfig.phonemeSilenceSeconds)[phoneme] =
			phonemeItem.value().get<float>();
		}
	    } // if phoneme_silence
	} // if inference
    }

// Copied from piper distribution.
    void parseModelConfig(json &configRoot, ModelConfig &modelConfig)
    {
	modelConfig.numSpeakers = configRoot["num_speakers"].get<SpeakerId>();
	if (configRoot.contains("speaker_id_map")) {
	    if (!modelConfig.speakerIdMap) {
		modelConfig.speakerIdMap.emplace();
	    }
	    auto speakerIdMapValue = configRoot["speaker_id_map"];
	    for (auto &speakerItem : speakerIdMapValue.items()) {
		std::string speakerName = speakerItem.key();
		(*modelConfig.speakerIdMap)[speakerName] =
		    speakerItem.value().get<SpeakerId>();
		//fprintf(stderr, "DLD| %lu speakerName %s\n",
		//speakerItem.value().get<SpeakerId>(), speakerName);
	    }
	}
    }

// Copied from piper distribution.
    void initialize(PiperConfig &config)
    {
	if (config.useESpeak) {
	    // Set up espeak-ng for calling espeak_TextToPhonemesWithTerminator
	    // See: https://github.com/rhasspy/espeak-ng
	    //debug("Initializing eSpeak");
	    int result = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, config.eSpeakDataPath.c_str(), 0);
	    if (result < 0) {
		throw std::runtime_error("Failed to initialize eSpeak-ng");
	    }
	}
	// Load onnx model for libtashkeel
	// https://github.com/mush42/libtashkeel/
	if (config.useTashkeel) {
	    //debug("Using libtashkeel for diacritization");
	    if (!config.tashkeelModelPath) {
		throw std::runtime_error("No path to libtashkeel model");
	    }
	    //debug("Loading libtashkeel model from {}",
	    //config.tashkeelModelPath.value());
	    config.tashkeelState = std::make_unique<tashkeel::State>();
	    tashkeel::tashkeel_load(config.tashkeelModelPath.value(),
				    *config.tashkeelState);
	}
	//info("Initialized piper");
    }

    void terminate(PiperConfig &config)
    {
	(void)cxxpiper_free_cbuf();
	(void)cxxpiper_free_voice_list();
	if (config.useESpeak) {
	    espeak_Terminate();
	}
	//piper::terminate(piperConfig);
    }

// Copied from piper distribution.
    void loadModel(std::string modelPath, ModelSession &session, bool useCuda)
    {
	//info("Loading onnx model from {}", modelPath);
	session.env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
			       instanceName.c_str());
	session.env.DisableTelemetryEvents();
	if (useCuda) {
	    // Use CUDA provider
	    OrtCUDAProviderOptions cuda_options{};
	    cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
	    session.options.AppendExecutionProvider_CUDA(cuda_options);
	}
	// Slows down performance by ~2x
	// session.options.SetIntraOpNumThreads(1);
	// Roughly doubles load time for no visible inference benefit
	// session.options.SetGraphOptimizationLevel(
	//     GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
	session.options.SetGraphOptimizationLevel(
	    GraphOptimizationLevel::ORT_DISABLE_ALL);
	// Slows down performance very slightly
	// session.options.SetExecutionMode(ExecutionMode::ORT_PARALLEL);
	session.options.DisableCpuMemArena();
	session.options.DisableMemPattern();
	session.options.DisableProfiling();
	//auto startTime = std::chrono::steady_clock::now();
	auto modelPathStr = modelPath.c_str();
	session.onnx =Ort::Session(session.env, modelPathStr, session.options);
	//auto endTime = std::chrono::steady_clock::now();
	//info("Loaded onnx model in {} second(s)",
	//std::chrono::duration<double>(endTime - startTime).count());
    }

// Copied from piper distribution.
    void loadVoice(PiperConfig &config, std::string modelPath,
		   std::string modelConfigPath, Voice &voice,
		   std::optional<SpeakerId> &speakerId, bool useCuda)
    {
	//debug("loadVoice: Parsing voice config from {}", modelConfigPath);
	std::ifstream modelConfigFile(modelConfigPath);
	voice.configRoot = json::parse(modelConfigFile);
	parsePhonemizeConfig(voice.configRoot, voice.phonemizeConfig);
	parseSynthesisConfig(voice.configRoot, voice.synthesisConfig);
	parseModelConfig(voice.configRoot, voice.modelConfig);
	if (voice.modelConfig.numSpeakers > 1) {
	    if (speakerId) {
		DBG(DBG_MODNAME "Using Multi-speaker model, found configured speaker id %d", speakerId);
		voice.synthesisConfig.speakerId = speakerId;
	    } else {
		DBG(DBG_MODNAME "Using Multi-speaker model with default speaker id 0");
		voice.synthesisConfig.speakerId = 0;
	    }
	}
	DBG(DBG_MODNAME "Model contains %d speaker(s)", voice.modelConfig.numSpeakers);
	loadModel(modelPath, voice.session, useCuda);
    }

// Copied from piper distribution.
    void synthesize(std::vector<PhonemeId> &phonemeIds,
		    SynthesisConfig &synthesisConfig, ModelSession &session,
		    std::vector<int16_t> &audioBuffer, SynthesisResult &result)
    {
	//debug("Synthesizing audio for {} phoneme id(s)", phonemeIds.size());
	auto memoryInfo = Ort::MemoryInfo::CreateCpu(
	    OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
	// Allocate
	std::vector<int64_t> phonemeIdLengths{(int64_t)phonemeIds.size()};
	std::vector<float> scales{synthesisConfig.noiseScale,
				  synthesisConfig.lengthScale,
				  synthesisConfig.noiseW};
	std::vector<Ort::Value> inputTensors;
	std::vector<int64_t> phonemeIdsShape{1, (int64_t)phonemeIds.size()};
	inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
				   memoryInfo, phonemeIds.data(), phonemeIds.size(), phonemeIdsShape.data(),
				   phonemeIdsShape.size()));
	std::vector<int64_t> phomemeIdLengthsShape{(int64_t)phonemeIdLengths.size()};
	inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
				   memoryInfo, phonemeIdLengths.data(), phonemeIdLengths.size(),
				   phomemeIdLengthsShape.data(), phomemeIdLengthsShape.size()));
	std::vector<int64_t> scalesShape{(int64_t)scales.size()};
	inputTensors.push_back(
	    Ort::Value::CreateTensor<float>(memoryInfo, scales.data(), scales.size(),
					    scalesShape.data(), scalesShape.size()));
	// Add speaker id.
	// NOTE: These must be kept outside the "if" below to avoid being deallocated.
	std::vector<int64_t> speakerId{
	    (int64_t)synthesisConfig.speakerId.value_or(0)};
	std::vector<int64_t> speakerIdShape{(int64_t)speakerId.size()};
	if (synthesisConfig.speakerId) {
	    inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
				       memoryInfo, speakerId.data(), speakerId.size(), speakerIdShape.data(),
				       speakerIdShape.size()));
	}
	// From export_onnx.py
	std::array<const char *, 4> inputNames = {"input", "input_lengths", "scales",
						  "sid"};
	std::array<const char *, 1> outputNames = {"output"};
	// Infer
	auto startTime = std::chrono::steady_clock::now();
	auto outputTensors = session.onnx.Run(
	    Ort::RunOptions{nullptr}, inputNames.data(), inputTensors.data(),
	    inputTensors.size(), outputNames.data(), outputNames.size());
	auto endTime = std::chrono::steady_clock::now();
	if ((outputTensors.size() != 1) || (!outputTensors.front().IsTensor())) {
	    throw std::runtime_error("Invalid output tensors");
	}
	auto inferDuration = std::chrono::duration<double>(endTime - startTime);
	result.inferSeconds = inferDuration.count();
	const float *audio = outputTensors.front().GetTensorData<float>();
	auto audioShape =
	    outputTensors.front().GetTensorTypeAndShapeInfo().GetShape();
	int64_t audioCount = audioShape[audioShape.size() - 1];
	result.audioSeconds = (double)audioCount / (double)synthesisConfig.sampleRate;
	result.realTimeFactor = 0.0;
	if (result.audioSeconds > 0) {
	    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
	}
	//fprintf(stderr, "Synthesized %f second(s) of audio in %f second(s)\n",
	//result.audioSeconds, result.inferSeconds);
	// Get max audio value for scaling
	float maxAudioValue = 0.01f;
	for (int64_t i = 0; i < audioCount; i++) {
	    float audioValue = abs(audio[i]);
	    if (audioValue > maxAudioValue) {
		maxAudioValue = audioValue;
	    }
	}
	// We know the size up front
	audioBuffer.reserve(audioCount);
	// Scale audio to fill range and convert to int16
	float audioScale = (MAX_WAV_VALUE / std::max(0.01f, maxAudioValue));
	for (int64_t i = 0; i < audioCount; i++) {
	    int16_t intAudioValue = static_cast<int16_t>(
		std::clamp(audio[i] * audioScale,
			   static_cast<float>(std::numeric_limits<int16_t>::min()),
			   static_cast<float>(std::numeric_limits<int16_t>::max())));
	    audioBuffer.push_back(intAudioValue);
	}
	// Clean up
	for (std::size_t i = 0; i < outputTensors.size(); i++) {
	    Ort::detail::OrtRelease(outputTensors[i].release());
	}
	for (std::size_t i = 0; i < inputTensors.size(); i++) {
	    Ort::detail::OrtRelease(inputTensors[i].release());
	}
    }

// Copied from piper . distribution.
    void textToAudio(PiperConfig &config, Voice &voice, std::string text,
		     std::vector<int16_t> &audioBuffer, SynthesisResult &result,
		     const std::function<void()> &audioCallback)
    {
	std::size_t sentenceSilenceSamples = 0;
	if (voice.synthesisConfig.sentenceSilenceSeconds > 0) {
	    sentenceSilenceSamples = (std::size_t)(
		voice.synthesisConfig.sentenceSilenceSeconds *
		voice.synthesisConfig.sampleRate * voice.synthesisConfig.channels);
	}
	if (config.useTashkeel) {
	    if (!config.tashkeelState) {
		throw std::runtime_error("Tashkeel model is not loaded");
	    }
	    //debug("Diacritizing text with libtashkeel: {}", text);
	    text = tashkeel::tashkeel_run(text, *config.tashkeelState);
	}
	// Phonemes for each sentence
	std::vector<std::vector<Phoneme>> phonemes;
	if (voice.phonemizeConfig.phonemeType == eSpeakPhonemes) {
	    // Use espeak-ng for phonemization
	    eSpeakPhonemeConfig eSpeakConfig;
	    eSpeakConfig.voice = voice.phonemizeConfig.eSpeak.voice;
	    phonemize_eSpeak(text, eSpeakConfig, phonemes);
	} else {
	    // Use UTF-8 codepoints as "phonemes"
	    CodepointsPhonemeConfig codepointsConfig;
	    phonemize_codepoints(text, codepointsConfig, phonemes);
	}
	// Synthesize each sentence independently.
	std::vector<PhonemeId> phonemeIds;
	std::map<Phoneme, std::size_t> missingPhonemes;
	for (auto phonemesIter = phonemes.begin(); phonemesIter != phonemes.end();
	     ++phonemesIter) {
	    std::vector<Phoneme> &sentencePhonemes = *phonemesIter;
	    std::vector<std::shared_ptr<std::vector<Phoneme>>> phrasePhonemes;
	    std::vector<SynthesisResult> phraseResults;
	    std::vector<size_t> phraseSilenceSamples;
	    // Use phoneme/id map from config
	    PhonemeIdConfig idConfig;
	    idConfig.phonemeIdMap =
		std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
	    if (voice.synthesisConfig.phonemeSilenceSeconds) {
		// Split into phrases
		std::map<Phoneme, float> &phonemeSilenceSeconds =
		    *voice.synthesisConfig.phonemeSilenceSeconds;
		auto currentPhrasePhonemes = std::make_shared<std::vector<Phoneme>>();
		phrasePhonemes.push_back(currentPhrasePhonemes);
		for (auto sentencePhonemesIter = sentencePhonemes.begin();
		     sentencePhonemesIter != sentencePhonemes.end();
		     sentencePhonemesIter++) {
		    Phoneme &currentPhoneme = *sentencePhonemesIter;
		    currentPhrasePhonemes->push_back(currentPhoneme);
		    if (phonemeSilenceSeconds.count(currentPhoneme) > 0) {
			// Split at phrase boundary
			phraseSilenceSamples.push_back(
			    (std::size_t)(phonemeSilenceSeconds[currentPhoneme] *
					  voice.synthesisConfig.sampleRate *
					  voice.synthesisConfig.channels));
			currentPhrasePhonemes = std::make_shared<std::vector<Phoneme>>();
			phrasePhonemes.push_back(currentPhrasePhonemes);
		    }
		}
	    } else {
		// Use all phonemes
		phrasePhonemes.push_back(
		    std::make_shared<std::vector<Phoneme>>(sentencePhonemes));
	    }
	    // Ensure results/samples are the same size
	    while (phraseResults.size() < phrasePhonemes.size()) {
		phraseResults.emplace_back();
	    }
	    while (phraseSilenceSamples.size() < phrasePhonemes.size()) {
		phraseSilenceSamples.push_back(0);
	    }
	    // phonemes -> ids -> audio
	    for (size_t phraseIdx = 0; phraseIdx < phrasePhonemes.size(); phraseIdx++) {
		if (phrasePhonemes[phraseIdx]->size() <= 0) {
		    continue;
		}
		// phonemes -> ids
		phonemes_to_ids(*(phrasePhonemes[phraseIdx]), idConfig, phonemeIds,
				missingPhonemes);
		// ids -> audio
		synthesize(phonemeIds, voice.synthesisConfig, voice.session, audioBuffer,
			   phraseResults[phraseIdx]);
		// Add end of phrase silence
		for (std::size_t i = 0; i < phraseSilenceSamples[phraseIdx]; i++) {
		    audioBuffer.push_back(0);
		}
		result.audioSeconds += phraseResults[phraseIdx].audioSeconds;
		result.inferSeconds += phraseResults[phraseIdx].inferSeconds;
		phonemeIds.clear();
	    }
	    // Add end of sentence silence
	    if (sentenceSilenceSamples > 0) {
		for (std::size_t i = 0; i < sentenceSilenceSamples; i++) {
		    audioBuffer.push_back(0);
		}
	    }
	    if (audioCallback) {
		// Call back must copy audio since it is cleared afterwards.
		audioCallback();
		audioBuffer.clear();
	    }
	    phonemeIds.clear();
	}
	if (missingPhonemes.size() > 0) {
	    //warn("Missing {} phoneme(s) from phoneme/id map!",
	    //missingPhonemes.size());
	    for (auto phonemeCount : missingPhonemes) {
		std::string phonemeStr;
		utf8::append(phonemeCount.first, std::back_inserter(phonemeStr));
		//warn("Missing \"{}\" (\\u{:04X}): {} time(s)", phonemeStr,
		//(uint32_t)phonemeCount.first, phonemeCount.second);
	    }
	}
	if (result.audioSeconds > 0) {
	    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
	}
    }

    RubberBandStretcher::Options initOptions()
    {
	RubberBandStretcher::Options options = 0;
	options |= RubberBandStretcher::OptionProcessRealTime;
	options |= RubberBandStretcher::OptionPhaseIndependent;
	switch (0) {
	case 0:
	    options |= RubberBandStretcher::OptionThreadingAuto;
	    break;
	case 1:
	    options |= RubberBandStretcher::OptionThreadingNever;
	    break;
	case 2:
	    options |= RubberBandStretcher::OptionThreadingAlways;
	    break;
	}
	options |= RubberBandStretcher::OptionTransientsCrisp;
	options |= RubberBandStretcher::OptionDetectorCompound;
    
	return options;
    }

    void internalizeSamples(vector<int16_t> ibuf, float** cbuf, const int channels, const int start, const int count)
    {
	//debug("Internalizing: ibuf.size: [], start: [], count: []", ibuf.size(), start, count);
	for (int c = 0; c < channels; ++c) {
	    for (int i = 0; i < count; ++i) {
		cbuf[c][i] = (float)(ibuf[start + (i * channels) + c]);
	    }
	    if (count < bs) {
		for (size_t i = count; i < bs; ++i) {
		    cbuf[c][i] = MIN_WAV_VALUE;
		}
	    }
	}
	++izCnt;
	izTot += count;
    }

    void externalizeSamples(float **cbuf, vector<int16_t>& ibuf, const float& gain, const int channels, const int start, const int count)
    {
	//debug("Externalizing: ibuf.size: [], start: [], count: []", ibuf.size(), start, count);
	for (int c = 0; c < channels; ++c) {
	    for (int i = 0; i < count; ++i) {
		float value = gain * cbuf[c][i];
		if (value < MIN_WAV_VALUE) value = MIN_WAV_VALUE;
		if (value > MAX_WAV_VALUE) value = MAX_WAV_VALUE;
		ibuf.push_back((int16_t)value);
	    }
	}
	++ezCnt;
	ezTot += count;
    }

    int adjust(const int samplerate, const int channels, double ratio, double pitchshift, float gain,
	       vector<int16_t>& audioBuffer, vector<int16_t>& sharedAudioBuffer)
    {
	DBG("adjust, ab size: %lu ratio: %f pitchshift %f gain %f", audioBuffer.size(), ratio, pitchshift, gain);
	const size_t abSize = audioBuffer.size();
	assert(abSize > 0);
	double frequencyshift = 1.0;
	if (pitchshift != 0.0) frequencyshift *= pow(2.0, pitchshift / 12.0);
	RubberBandStretcher ts(samplerate, channels, initOptions(), ratio, frequencyshift);
	ts.setExpectedInputDuration(audioBuffer.size());
	ts.setMaxProcessSize(bs);
	size_t countIn = 0;
	size_t countOut = 0;
	// "ib" stands for "input block", "ob" for "output block".
	size_t ibSize = bs;
	size_t ibLastFullIdx = abSize / bs;
	assert(ibLastFullIdx >= 0);
	for (size_t ibIdx = 0; ibIdx < ibLastFullIdx; ++ibIdx) {
	    assert(countIn % bs == 0);
	    ibSize = (ibIdx == ibLastFullIdx) ? abSize - countIn : bs;
	    assert(ibSize >= 0);
	    assert(ibSize > 0);
	    assert(ibSize <= bs);
	    internalizeSamples(audioBuffer, cbuf, channels, countIn, ibSize);
	    countIn += ibSize;
	    //DBG("About to ts.process, countIn is now %lu, final? %d", countIn, ibIdx == ibLastFullIdx);
	    ts.process(cbuf, ibSize, ibIdx == ibLastFullIdx);
	    ++prCnt;
	    prTot += ibSize;
	    int avail;
	    while ((avail = ts.available()) > 0) {
		// All data processed for this block.
		if (avail == -1) break;
		size_t obSize = bs;
		const size_t obLastFullIdx = avail / bs;
		// Do all the full blocks.
		for (size_t obIdx = 0; obIdx < obLastFullIdx; ++obIdx) {
		    assert(obSize <= bs);
		    ts.retrieve(cbuf, obSize);
		    (void)externalizeSamples(cbuf, sharedAudioBuffer, gain, channels, obIdx * bs, obSize);
		    countOut += obSize;
		}
		// do any remaining partial block.
		size_t remaining = avail - (obLastFullIdx * bs);
		assert(remaining >= 0);
		if (remaining > 0) {
		    //DBG("Retrieving remaining: %lu", remaining);
		    assert(remaining <= bs);
		    ts.retrieve(cbuf, remaining);
		    (void)externalizeSamples(cbuf, sharedAudioBuffer, gain, channels, obLastFullIdx* bs, remaining);
		    countOut += remaining;
		}
	    }
	    // All data processed, all done.
	    if (avail == -1) break;
	}
	return 0;
    }

    extern "C" {

	// OutputType and RunConfig are copied from piper and are , needed for synthesis
	// and populated from the arguments to piper's command line demo
	// program 'piper'.  We, as an SD oOM, populate this mostly from
	// the output module config file, cxxpiper.conf .  But some stuff
	// like speaker id may be updated/overridden per-query from SSIP data.
	enum OutputType { OUTPUT_FILE, OUTPUT_DIRECTORY, OUTPUT_STDOUT, OUTPUT_RAW };
	struct RunConfig {
	    // Path to .onnx voice file
	    filesystem::path modelPath;
	    // Path to JSON voice config file
	    filesystem::path modelConfigPath;
	    char* defaultVoiceName;	
	    OutputType outputType = OUTPUT_STDOUT;
	    optional<filesystem::path> outputPath = filesystem::path(".");
	    // Numerical id of the default speaker (multi-speaker voices)
	    optional<piper::SpeakerId> speakerId;
	    optional<float> noiseScale;
	    // Speed of speaking (1 = normal, < 1 is faster, > 1 is slower)
	    optional<float> lengthScale;
	    optional<float> noiseW;
	    // Seconds of silence to add after each sentence
	    optional<float> sentenceSilenceSeconds;
	    // Path to espeak-ng data directory (default is next to piper executable)
	    optional<filesystem::path> eSpeakDataPath;
	    // Path to libtashkeel ort model
	    optional<filesystem::path> tashkeelModelPath;
	    bool jsonInput = false;
	    // Seconds of extra silence to insert after a single phoneme
	    optional<std::map<piper::Phoneme, float>> phonemeSilenceSeconds;
	    // true to use CUDA execution provider
	    bool useCuda = false;
	};
	static RunConfig runConfig;
	static piper::Voice voice;
	static SPDVoice **cxxpiper_voice_list = NULL;

	static int cxxpiper_alloc_cbuf();
	static SPDVoice **cxxpiper_allocate_voice_list();
	static void cxxpiper_free_voice_list();
	static void cxxpiper_handle_text(const char *);
	static void cxxpiper_handle_sound_icon(const char *);
	static void cxxpiper_set_language(char *);
	static void cxxpiper_set_synthesis_voice(char *voice_name);
	static void cxxpiper_set_voice_type(SPDVoiceType);
	static int cxxpiper_voice_name_to_speaker_id(const char *);

	MOD_OPTION_1_INT(UseCUDA)
	MOD_OPTION_1_STR(ModelPath)
	MOD_OPTION_1_STR(ConfigPath)
	MOD_OPTION_1_STR(SoundIconFolder)
	MOD_OPTION_1_INT(SoundIconVolume)
	// SIC: see below regarding NONE and ALL
	MOD_OPTION_1_STR(PunctSome)
	MOD_OPTION_1_STR(PunctMost)
	MOD_OPTION_1_STR(ESpeakNGDataDirPath)

	int module_load(void)
	{
	    INIT_SETTINGS_TABLES();
	    REGISTER_DEBUG();
	    MOD_OPTION_1_INT_REG(UseCUDA, 0);
	    MOD_OPTION_1_STR_REG(ModelPath, "");
	    MOD_OPTION_1_STR_REG(ConfigPath, "");
	    MOD_OPTION_1_STR_REG(SoundIconFolder, "/usr/share/sounds/sound-icons/");
	    MOD_OPTION_1_INT_REG(SoundIconVolume, 0);
	    // NB:  NONE and ALL are constant so no config.  NONE means omit as much as possible
	    // while ALL  means omit as little as possible.
	    // NB: Don't provide defaults, since empty string means fallback to SD's definitions.
	    MOD_OPTION_1_STR_REG(PunctSome, "");
	    MOD_OPTION_1_STR_REG(PunctMost, "");
	    MOD_OPTION_1_STR_REG(ESpeakNGDataDirPath, "/usr/share/espeak-ng-data/");
	    module_register_available_voices();
	    module_register_settings_voices();
	    return 0;
	}

	int module_init(char **status_info)
	{
	    module_audio_set_server();
	    module_audio_init(status_info);
	    runConfig.useCuda = UseCUDA;
	    runConfig.modelPath = filesystem::path(ModelPath);
	    runConfig.modelConfigPath = filesystem::path(ConfigPath);
	    runConfig.eSpeakDataPath = filesystem::path(ESpeakNGDataDirPath);
	    runConfig.outputPath = nullopt;
	    char *default_voice = module_getdefaultvoice();
	    runConfig.defaultVoiceName = default_voice;
	    runConfig.speakerId = cxxpiper_voice_name_to_speaker_id(runConfig.defaultVoiceName);
	    DBG("Default Voice is %s", runConfig.defaultVoiceName);
	    try {
		cxxpiper::loadVoice(piperConfig, runConfig.modelPath.string(), runConfig.modelConfigPath.string(),
				    voice, runConfig.speakerId, runConfig.useCuda);
		if (voice.phonemizeConfig.phonemeType == piper::eSpeakPhonemes) {
		    piperConfig.eSpeakDataPath = runConfig.eSpeakDataPath.value().string();
		}
		else {
		    piperConfig.useESpeak = false;
		}
		cxxpiper::initialize(piperConfig);
		cxxpiper_voice_list = cxxpiper_allocate_voice_list();
		cxxpiper_alloc_cbuf();
		*status_info = g_strdup(DBG_MODNAME " Initialized successfully.");
	    }
	    catch (const Ort::Exception& e) {
		DBG(DBG_MODNAME " Could not initialize,caught onnx runtime (model) exception: %s", e.what());
		*status_info = g_strdup(e.what());
		return -1;
	    }
	    catch (const json::parse_error& e) {
		DBG(DBG_MODNAME " Could not initialize,caught JSON exception: %s", e.what());
 		*status_info = g_strdup(e.what());
		return -1;
	    }
	    catch (const std::runtime_error& e) {
		DBG(DBG_MODNAME " Could not initialize,caught runtime_error exception: %s", e.what());
 		*status_info = g_strdup(e.what());
		return -1;
	    }
	    return 0;
	}

	SPDVoice **module_list_voices(void)
	{
	    return cxxpiper_voice_list;
	}

	void module_speak_sync(const char *data, size_t bytes, SPDMessageType msgtype)
	{
	    stop_requested = 0;
	    UPDATE_STRING_PARAMETER(voice.language, cxxpiper_set_language);
	    UPDATE_PARAMETER(voice_type, cxxpiper_set_voice_type);
	    UPDATE_STRING_PARAMETER(voice.name, cxxpiper_set_synthesis_voice);
	    module_speak_ok();
	    switch (msgtype) {
	    case SPD_MSGTYPE_CHAR:
	    case SPD_MSGTYPE_KEY:
	    case SPD_MSGTYPE_SPELL:
	    case SPD_MSGTYPE_TEXT:
		cxxpiper_handle_text(data);
		break;
	    case SPD_MSGTYPE_SOUND_ICON:
		cxxpiper_handle_sound_icon(data);
		break;
	    }
	}

	int module_stop(void)
	{
	    stop_requested = 1;
	    return 0;
	}

	size_t module_pause(void)
	{
	    stop_requested = 1;
	    return 0;
	}

	int module_close()
	{
	    if (piperConfig.useESpeak) espeak_Terminate();
	    (void)cxxpiper::terminate(piperConfig);
	    return 0;
	}

	static int cxxpiper_alloc_cbuf()
	{
	    const int channels = voice.synthesisConfig.channels;
	    cbuf = g_new0(float *, channels);
	    if ( ! cbuf) {
		throw std::runtime_error("Failure allocating dynamic memory for adjustment buffer");
	    }
	    for (size_t c = 0; c < channels; ++c) {
		cbuf[c] = g_new0(float, bs);
		if ( ! cbuf[c]) {
		    throw std::runtime_error("Failure allocating dynamic memory for adjustment buffer values");
		}
	    }
	    return 0;
	}

	static int cxxpiper_free_cbuf()
	{
	    const int channels = voice.synthesisConfig.channels;
	    for (size_t c = 0; c < channels; ++c) g_free(cbuf[c]);
	    g_free(cbuf);
	    return 0;
	}

	static SPDVoice **cxxpiper_allocate_voice_list()
	{
	    SPDVoice **result = NULL;
	    int num_spkrs = voice.configRoot["num_speakers"];
	    DBG("Model config reports num spkrs %d", num_spkrs);
	    // result consistes of an array of SPDVoice* addresses,
	    // one address per voice and one more NULL address as
	    // delimiter.  The non-NULL elements point to SPDVoice
	    // structs and we'll dynamically allocate memory for them,
	    // below.  First the "root" array:
	    result = g_new0(SPDVoice *, num_spkrs + 1);
	    if ( ! result) {
		throw std::runtime_error("Failure allocating dynamic memory for voice list");
	    }
	    if (num_spkrs > 1) {
		if ( ! voice.configRoot.contains("speaker_id_map")) {
		    //warn("ERROR: Using multispeaker model but could not find speaker_id_map in json model configureation file");
		    //throw std::runtime_error("Using multispeaker model but could not find speaker_id_map in json model configureation file");
		}
		auto speakerIdMapValue = voice.configRoot["speaker_id_map"];
		DBG("Model config speaker_id_map has %lu entries", speakerIdMapValue.size());
		SPDVoice **reg_voices = module_list_registered_voices();
		int i = 0;
		for (auto &speakerItem : speakerIdMapValue.items()) {
		    std::string spkr_name = speakerItem.key();
		    int spkr_id = speakerItem.value().get<SpeakerId>();
		    // Dynamically allocate an SPDVoice and point to it from result[[], the "root" array.
		    SPDVoice *v = g_new0(SPDVoice, 1);
		    v->name = g_strdup_printf("%s~%d~%s", runConfig.modelPath.stem().string().c_str(), spkr_id, spkr_name.c_str());
		    v->language = g_strdup(voice.configRoot["language"]["code"].get<std::string>().c_str());
		    char *var_str = NULL;
		    for (int j = 0; reg_voices && reg_voices[j]; ++j) {
			SPDVoice* r_v = reg_voices[j];
			if (strncasecmp(v->name, r_v->name, strlen(v->name)) == 0) {
			    var_str = r_v->variant;
			    DBG("matched configured voice type %s with voice %s", var_str, r_v->name);
			    break;
			}
		    }
		    if (strncasecmp(v->name, runConfig.defaultVoiceName, strlen(v->name)) == 0) {
			v->variant = g_strdup((var_str == NULL) ? "(default)" : strcat(var_str, " (default)"));
		    }
		    else {
			v->variant = g_strdup((var_str == NULL) ? "" : var_str);
		    }
		    result[i] = v;
		    ++i;
		}
	    }
	    else {
		// Theoretically, there is no need to copy voice type string into varient b/c there's only one voice
		// and it will be used.  However, voice list should show the type string, assuming proper AddVoice configuration.
		DBG("Running with single-speaker model");
		SPDVoice *v = g_new0(SPDVoice, 1);
		v->name = g_strdup(runConfig.modelPath.stem().string().c_str());
		v->language = g_strdup(voice.configRoot["language"]["code"].get<std::string>().c_str());
		v->variant = g_strdup("");
		result[0] = v;
	    }
	    // Delimit the "root array.
	    result[num_spkrs] = NULL;
	    return result;
	}

	static void cxxpiper_free_voice_list()
	{
#ifdef ESPEAK_NG_INCLUDE
	    free(espeak_variants_array);
#endif
	    if (cxxpiper_voice_list != NULL) {
		for (int i = 0; cxxpiper_voice_list[i] != NULL; i++) {
		    g_free(cxxpiper_voice_list[i]->name);
		    g_free(cxxpiper_voice_list[i]->language);
		    g_free(cxxpiper_voice_list[i]->variant);
		    g_free(cxxpiper_voice_list[i]);
		}
		g_free(cxxpiper_voice_list);
		cxxpiper_voice_list = NULL;
	    }
	}

	static char *cxxpiper_voice_enum_to_str(SPDVoiceType voice_type)
	{
	    char *voicename;
	    switch (voice_type) {
	    case SPD_MALE1:
		voicename = g_strdup("MALE1");
		break;
	    case SPD_MALE2:
		voicename = g_strdup("MALE2");
		break;
	    case SPD_MALE3:
		voicename = g_strdup("MALE3");
		break;
	    case SPD_FEMALE1:
		voicename = g_strdup("FEMALE1");
		break;
	    case SPD_FEMALE2:
		voicename = g_strdup("FEMALE2");
		break;
	    case SPD_FEMALE3:
		voicename = g_strdup("FEMALE3");
		break;
	    case SPD_CHILD_MALE:
		voicename = g_strdup("CHILD_MALE");
		break;
	    case SPD_CHILD_FEMALE:
		voicename = g_strdup("CHILD_FEMALE");
		break;
	    case SPD_UNSPECIFIED:
		// SIC:  fall through to default
	    default:
		voicename = NULL;
		break;
	    }
	    return voicename;
	}

	static const char *cxxpiper_search_for_sound_icon(const char *icon_name)
	{
	    char *fn = NULL;
	    if (strlen(SoundIconFolder) == 0) return fn;
	    GString *filename = g_string_new(SoundIconFolder);
	    filename = g_string_append(filename, icon_name);
	    if (g_file_test(filename->str, G_FILE_TEST_EXISTS)) fn = filename->str;
	    /* if the file was found, the pointer *fn  points to the character data
	       of the string filename. In this situation the string filename must be
	       freed but its character data must be preserved.
	       If the file is not found, the pointer *fn contains NULL. In this
	       situation the string filename must be freed, including its character
	       data.
	    */
	    return g_string_free(filename, (fn == NULL));
	}

	void cxxpiper_stretch_and_copy(const int samplerate, const int channels, vector<int16_t>& audioBuffer, vector<int16_t>& sharedAudioBuffer)
	{
	    // In rubberband stretcher units, 1.0 is no duration change.
	    // Map -100 .. 100 onto 0.0 .. 2.0 .
	    double ratio = (((double)msg_settings.rate) + 100.0) / 100.0;
	    // Pitch is converted to frequency internally, as per 'piper'
	    // command line utility.  The 'adjust' function uses the same
	    // conversion.  I think -10.0 to 10.0
	    // provides a usable range plus a bit of margin.
	    // .  Map -100 .. 100 onto -10.0 .. 10.0
	    // Also clamp input values otherwise we will crash!
	    double mS = ((double)msg_settings.pitch);
	    if (mS < -100.0) mS = -100.0;
	    if (mS > 100.0) mS = 100.0;
	    double pitchshift = mS / 10.0;
	    //  In rubberband stretcher units, 1.0 is no gain/attenuation.
	    // Map -100 .. 100 onto 0.0 .. 1.0
	    float gain = (((float)msg_settings.volume) + 100.0f) / 200.0f;
	    (void)adjust(samplerate, channels, ratio, pitchshift, gain, audioBuffer, sharedAudioBuffer);
	}

	static void cxxpiper_handle_text(const char *data)
	{
	    DBG("Input data before strip XML: %s", data);
	    cmdInp = (char *) module_strip_ssml(data);
	    DBG("Input after strip XML: %s", cmdInp);
	    mutex mutAudio;
	    condition_variable cvAudio;
	    bool audioReady = false;
	    cbCnt = 0;
	    cbTot = 0;
	    prCnt = 0;
	    prTot = 0;
	    izCnt =0;
	    izTot =0;
	    ezCnt =0;
	    ezTot =0;
	    runConfig.lengthScale = msg_settings.rate / 100.0;
	    vector<int16_t> audioBuffer;
	    vector<int16_t> sharedAudioBuffer;
	    piper::SynthesisResult result;
	    auto audioCallback = [&audioBuffer, &sharedAudioBuffer, &mutAudio,
				  &cvAudio, &audioReady]()
	    {
		unique_lock lockAudio(mutAudio);
#if 0
		copy(audioBuffer.begin(), audioBuffer.end(), back_inserter(sharedAudioBuffer));
#else
		cxxpiper_stretch_and_copy(voice.synthesisConfig.sampleRate, voice.synthesisConfig.channels, audioBuffer, sharedAudioBuffer);
#endif
		audioReady = true;
		++cbCnt;
		cbTot += audioBuffer.size();	    //cerr << "CVaudio notivy 1" << endl;
		cvAudio.notify_one();
	    };
	    DBG("Sending begin event");
	    module_report_event_begin();
	    (void)cxxpiper::textToAudio(piperConfig, voice, cmdInp,
					audioBuffer, result, audioCallback);
	    DBG("Did synthesis, ab size is %lu sab size is %lu",
		audioBuffer.size(), sharedAudioBuffer.size());
	    AudioFormat format = SPD_AUDIO_LE;
	    AudioTrack track;
	    track.bits = voice.synthesisConfig.sampleWidth * 8;
	    track.num_samples = sharedAudioBuffer.size();
	    track.samples = &sharedAudioBuffer[0]; // outbuf;
	    track.num_channels = voice.synthesisConfig.channels;
	    track.sample_rate = voice.synthesisConfig.sampleRate;
	    DBG("callback called %lu times, total: %lu", cbCnt, cbTot);
	    DBG("process called %lu times, total: %lu", prCnt, prTot);
	    DBG("internalize called %lu times, total: %lu", izCnt, izTot);
	    DBG("externalize called %lu times, total: %lu", ezCnt, ezTot);
	    DBG("sending track to audio output");
	    module_tts_output_server(&track, format);
	    DBG("Sending end event");
	    module_report_event_end();
	}

	static void cxxpiper_handle_sound_icon(const char *icon_name)
	{
	    int fallback_to_speech = 0;
	    const char *icon_path = cxxpiper_search_for_sound_icon(icon_name);
	    if (icon_path != NULL) {
		module_report_event_begin();
		(void)module_play_file(icon_path);
		module_report_icon(icon_path);
		module_report_event_end();
	    }
	    else {
		fallback_to_speech = 1;
	    }
	    if (fallback_to_speech) {
		MSG(3, "Warning:  Speaking sound icon name, %s, as a fallback, since audio file can not be found.", icon_name);
		cxxpiper_handle_text(icon_name);
	    }
	}

	static void cxxpiper_set_language_and_voice(char *lang, SPDVoiceType voice_type, char *name);
	static void cxxpiper_set_language_and_voice(char *lang, SPDVoiceType voice_type, char *name)
	{
	    int i = 0;
	    int index = -1;
	    char *tstr = cxxpiper_voice_enum_to_str(voice_type);

	    DBG("%s, lang=%s, voice_type=%d, name=%s",
		__FUNCTION__, lang, (int)voice_type, name ? name : "");
	    // -1 is explicitly "unspecified", 0 is plain old undefined.
	    if (voice_type > 0) {
		// The idea is that if the user submits a query with a
		// type, we should honor that type even if we can't
		// find a language match.  We prefer language and type
		// to match, of course.  Name is insignificant when type
		// is included except that if we find no type match at
		// all we fall back to language/name matching, as if
		// no type were given in the query.
		SPDVoice **vs = module_list_registered_voices();
		SPDVoice *first_close_match = NULL;
		for (int i = 0; vs && vs[i]; ++i) {
		    SPDVoice* v = vs[i];
		    DBG("typematch voice name: %s lang: %s variant: %s", v->name, v->language, v->variant);
		    int type_match_p = strncasecmp(tstr, v->variant, strlen(tstr));
		    if (type_match_p == 0 &&
			strncasecmp(lang, v->language, strlen(lang)) == 0) {
			DBG("strong match on language and type, $voice is now %s.", v->name);
			voice.synthesisConfig.speakerId = cxxpiper_voice_name_to_speaker_id(v->name);
			runConfig.speakerId = voice.synthesisConfig.speakerId;
			return;
		    }
		    else if (type_match_p) {
			first_close_match = v;
		    }
		}
		if (first_close_match) {
		    DBG("Weak match on type only!  Assuming configuration is sane, new voice is %s", first_close_match->name);
		    voice.synthesisConfig.speakerId = cxxpiper_voice_name_to_speaker_id(first_close_match->name);
		    runConfig.speakerId = voice.synthesisConfig.speakerId;
		    return;
		}
	    }
	    SPDVoice **lst = cxxpiper_voice_list;
	    if (name && *name) {
		for (i = 0; lst[i]; ++i) {
		    if (!strcasecmp(lst[i]->name, name)) {
			voice.synthesisConfig.speakerId = cxxpiper_voice_name_to_speaker_id(lst[i]->name);
			runConfig.speakerId = voice.synthesisConfig.speakerId;
			index = i;
			break;
		    }
		}
	    }
	    if ((index == -1) && lang) {
		char *langbase;	// requested base language + '-'
		char *dash = strchr(lang, '-');
		if (dash)
		    langbase = g_strndup(lang, dash-lang+1);
		else
		    langbase = g_strdup_printf("%s-", lang);
		for (i = 0; lst[i]; ++i) {
		    if (!strcasecmp(lst[i]->language, lang)) {
			DBG("strong match on language new voice is %s", lst[i]->name);
			voice.synthesisConfig.speakerId = cxxpiper_voice_name_to_speaker_id(lst[i]->name);
			runConfig.speakerId = voice.synthesisConfig.speakerId;
			index = i;
			break;
		    }
		    if (index == -1) {
			/* Try base language matching as fallback */
			if (!strncasecmp(lst[i]->language, langbase, strlen(langbase))) {
			    DBG("Base language match, new voice is %s", lst[i]->name);
			    voice.synthesisConfig.speakerId = cxxpiper_voice_name_to_speaker_id(lst[i]->name);
			    runConfig.speakerId = voice.synthesisConfig.speakerId;
			    index = i;
			}
		    }
		}
		g_free(langbase);
	    }
	    if (index == -1) { // no matching voice: choose the first available voice 
		if (!cxxpiper_voice_list[0])
		    voice.synthesisConfig.speakerId = cxxpiper_voice_name_to_speaker_id(cxxpiper_voice_list[0]->name);
		runConfig.speakerId = voice.synthesisConfig.speakerId;
		index = 0;
		return;
	    }
	}

	static void cxxpiper_set_voice_type(SPDVoiceType voice_type)
	{
	    const char *vt_str = cxxpiper_voice_enum_to_str(voice_type);
	    // NB: spd-say -t female1 twice in a row results in voice_type 4 in the first request, then -1 in the second!!
	    SPDVoice **vs = module_list_registered_voices();
	    if (vs != NULL) {
		if (vt_str == NULL) {
		    MSG(3, "Warning:  voice type from server is NULL!!  Should not happen.");
		    return;
		}
		for (int i = 0; vs[i] != NULL; i++) {
		    const char *vari = vs[i]->variant;
		    if (strlen(vari) <= strlen(vt_str) &&
			strncasecmp(vt_str, vari, strlen(vt_str)) == 0) {
			int spkr_id = cxxpiper_voice_name_to_speaker_id(vs[i]->name);
			voice.synthesisConfig.speakerId = spkr_id;
			runConfig.speakerId = spkr_id;
			return;
		    }
		}
		MSG(3, "Warning:  No definition of type %s, check 'AddVoice' directives in cxxpiper.conf file.", vt_str);
	    }
	    else {
		MSG(3, "Warning:  no configuredc voice types, please use AddVoice in cxxpiper.conf to define voice types needed at runtime.");
	    }
	}

	static void cxxpiper_set_language(char *lang)
	{
	    cxxpiper_set_language_and_voice(lang, msg_settings.voice_type, NULL);
	}

	static void cxxpiper_set_synthesis_voice(char *voice_name)
	{
	    if (voice_name == NULL) {
		return;
	    }
	    int spkr_id = -1;
	    if (msg_settings.voice.name != NULL) {
		spkr_id = cxxpiper_voice_name_to_speaker_id(msg_settings.voice.name);
	    }
	    else {
		spkr_id = cxxpiper_voice_name_to_speaker_id(runConfig.defaultVoiceName); // cxxpiper_voice_name_to_speaker_id(default_voice);
	    }
	    voice.synthesisConfig.speakerId = spkr_id;
	    runConfig.speakerId = voice.synthesisConfig.speakerId;
	    return;
	}

	static int cxxpiper_voice_name_to_speaker_id(const char *voice_name)
	{
	    // The form of a voice name is:
	    // <model-name>~{}<speaker-id>~<speaker-name>
	    int spkr_id;
	    int num_matches = sscanf(voice_name, "%*[^~]~%d~%*s", &spkr_id);
	    if (num_matches != 1) return -1;
	    return spkr_id;
	}

    } // extern "C"
} // namespace cxxpiperb
