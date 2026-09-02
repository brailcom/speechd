/*
The MIT License (MIT)

Copyright © 2026 John Settlemyer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "kitten.h"

int model_type=0;
int ROWS = 0;

// setting var's
float speed = 1.0f;
//['Leo','Kiki','Hugo','Rosie','Bruno','Luna','Jasper','Bella']
GString *voice;
GString *voice_setting;
// paths var's
GString *model_path;
GString *voices_path;
const char *home_dir;
GString *model_dir;

bool stop_generation=false;

// holds and array of values that must be passed to the model based off the requested voice and length of the text.
float *voice_styles;

// model var's
const OrtApi* g_ort = NULL;
OrtEnv* env = NULL;
OrtSessionOptions* session_options = NULL;
OrtSession* session = NULL;

/*
The original voices bins come from the kitten tts repo but were in the formate
of a npz which is difficult to process in c. They are really only named tensors.
So I unpacked them and turned them into arrays of floats that can be easly
loaded in c. For reference here is the python script to generate them from the
original npz files.

wget https://huggingface.co/KittenML/kitten-tts-nano-0.2/resolve/main/voices.npz
wget https://huggingface.co/KittenML/kitten-tts-micro-0.8/resolve/main/voices.npz
wget https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/voices.npz

import numpy as np
voices = np.load("voices.npz")
output = b""
for k in voices.keys():
    print(k)
    output = output+voices[k].tobytes()

with open("voice.bin",'wb') as op:
    op.write(output)
*/
int init_voice_style(const char* voices_path){
    // Load "voices.bin"
    gchar *file_contents = NULL;
    gsize file_length = 0;
    GError *error = NULL;

    if (!g_file_get_contents(voices_path, &file_contents, &file_length, &error)) {
        MSG(2, "ERROR: reading file: %s\n", error->message);
        g_clear_error(&error);
        return -1;
    }

    //2048 for nano
    if (file_length == 2048 * sizeof(float)) {
        model_type = 1;
        ROWS = 8;
    }
    //819200 for mini
    else if (file_length == 819200 * sizeof(float)) {
        model_type = 2;
        ROWS = 3200;
    }
    else {
        MSG(2, "ERROR: File size is incorrect must be 3200x256 floats or 8x256 floats.\n");
        g_free(file_contents);
        return -1;
    }

    voice_styles = (float *)file_contents;
    return 0;
}

void cleanup_voice_style(){
    // Clean up memory
    g_free(voice_styles);
}

// voices style loads and array of 256 floats that will get passed to the model
// to determine how it styles the voice. The style will change based of what
// voice you have selected and how long your text lenght is. But on the nano model
// It is simply a change in voice, the length is not used.
GArray *get_style(const char *text, const char *voice) {
    const char *voices[] = {
        "Leo", "Kiki", "Hugo", "Rosie",
        "Bruno", "Luna", "Jasper", "Bella"
    };
    gsize num_voices = G_N_ELEMENTS(voices);

    // Calculate ref_id
    gsize text_len = g_utf8_strlen(text, -1);
    int ref_id = (text_len < 399) ? (int)text_len : 399;
    if (model_type == 1){
        ref_id = 0; //nano model does not change style based off text length
    }

    // Find index of 'voice' in 'voices'
    int voice_index = -1;
    for (gsize i = 0; i < num_voices; i++) {
        if (g_strcmp0(voice, voices[i]) == 0) {
            voice_index = (int)i; // 0-based index
            break;
        }
    }

    if (voice_index == -1) {
        MSG(2, "ERROR: Voice '%s' not found in voices list.\n", voice);
        return NULL;
    }

    // Calculate target index in voice_styles
    // Formula: (voice_index * 400) + ref_id
    int target_row;
    if (model_type == 1){
        target_row = voice_index;
    }
    else {
        target_row = (voice_index * 400) + ref_id;
    }

    if (target_row < 0 || target_row >= ROWS) {
        MSG(2, "ERROR: Calculated row index %d out of bounds.\n", target_row);
        return NULL;
    }

    // Pointer to the specific 256-float block
    float *selected_voice_style = &voice_styles[target_row * COLS];

    // create our output array from the selection.
    GArray *output_array = g_array_new(FALSE, FALSE, sizeof(gfloat));
    g_array_append_vals(output_array, selected_voice_style, COLS);//should always take 256 values

    return output_array;
}

// The model will take your text in as char indices based off a fixed
// index string, the index string has been set above as SYMBOLS
// Which will get passed to this function in as the index_str
// the out will be an array of int's each being an indice.
GArray *get_char_indices(const gchar *locate, const gchar *index_str) {
    if (locate == NULL || index_str == NULL) {
        return NULL;
    }

    gsize char_length = g_utf8_strlen(locate, -1);
    if (char_length >= 400){
        MSG(2, "ERROR: string is over 400 char's long, this indice will not run\n");
        return NULL;
    }

    GArray *output_array = g_array_sized_new(FALSE, FALSE, sizeof(int64_t), char_length);

    // Starting value should be a 0.
    g_array_append_val(output_array, (int64_t){0});

    const gchar *curr = locate;
    for (gsize i = 0; i < char_length; i++) {
        // Extract the 32-bit Unicode code point (gunichar) at the current position
        gunichar ch = g_utf8_get_char(curr);

        // Search for the character in index_str
        gchar *match = g_utf8_strchr(index_str, -1, ch);

        int64_t index_val;
        if (match != NULL) {
            // Calculate character offset
            index_val = (int64_t)g_utf8_pointer_to_offset(index_str, match);
        } else {
            index_val = 16; // Character not found, so change it to a space character.
        }

        // Append the index value to the GArray
        g_array_append_val(output_array, index_val);

        // Advance to the next UTF-8 character.
        curr = g_utf8_next_char(curr);
    }

    // Ending values should always be 10 and 0
    g_array_append_val(output_array, (int64_t){10});
    g_array_append_val(output_array, (int64_t){0});

    return output_array;
}

// we use espeak's phoneme function to generate phonemes for the model.
// the phonemes don't get passed directly to the model but turned into
// indices later on.
GString *get_phonemes(const char *text){
    // Initialize espeak.
    int samplerate = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, NULL, 0);
    if (samplerate < 0) {
        MSG(2, "Error: Failed to initialize eSpeak.\n");
        return NULL;
    }

    // Set the voice to US English
    if (espeak_SetVoiceByName("en-us") != EE_OK) {
        MSG(2, "Error: Failed to set voice to en-us.\n");
        return NULL;
    }

    const void *text_ptr = (const void *)text;

    // Loop through the text to translate it into phonemes.
    int textmode = espeakCHARS_AUTO;

    // phonememode = 0x02 triggers IPA (equivalent to how Python's phonemizer acts by default)
    int phonememode = 0x02;

    // Create a new empty GString wrapper
    GString *combined_phonemes = g_string_new("");

    while (text_ptr != NULL) {
        const char *phonemes = espeak_TextToPhonemes(&text_ptr, textmode, phonememode);
        // phonemes is a single char we will have to combine them for the full string
        if (phonemes != NULL && *phonemes != '\0') {
            g_string_append(combined_phonemes, phonemes);
        }
    }

    return combined_phonemes;
}

// we create the tensor that onnx use as input here.
// this tensor will map to our phoneme text inputs indices.
OrtValue* create_inputs_tensor(OrtMemoryInfo* memory_info, GArray *inputs_array){
    // set up the input ids tensor.
    int64_t *input_ids_data = (int64_t *)inputs_array->data;
    guint elt_size = g_array_get_element_size(inputs_array);
    gsize total_bytes = (gsize)inputs_array->len * elt_size;
    int64_t input_ids_dims[] = {1, inputs_array->len};
    size_t input_ids_dims_len = 2;
    OrtValue* input_ids_tensor = NULL;
    CHECK_STATUS(g_ort->CreateTensorWithDataAsOrtValue(
        memory_info, input_ids_data, total_bytes,
        input_ids_dims, input_ids_dims_len, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &input_ids_tensor));
    return input_ids_tensor;
}

// this tensor maps to the choose voice style.
OrtValue* create_styles_tensor(OrtMemoryInfo* memory_info, GArray *styles_array){
    // set up the styles tensor.
    float *style_data = (float *)styles_array->data;
    guint elt_size2 = g_array_get_element_size(styles_array);
    gsize total_bytes2 = (gsize)styles_array->len * elt_size2;
    int64_t style_dims[] = {1, 256};
    size_t style_dims_len = 2;
    OrtValue* style_tensor = NULL;
    CHECK_STATUS(g_ort->CreateTensorWithDataAsOrtValue(
        memory_info, style_data, total_bytes2,
        style_dims, style_dims_len, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &style_tensor));
    return style_tensor;
}

OrtValue* create_speed_tensor(OrtMemoryInfo* memory_info, float* speed){
    float *speed_data = speed;
    int64_t speed_dims[] = {1};
    size_t speed_dims_len = 1;
    OrtValue* speed_tensor = NULL;
    CHECK_STATUS(g_ort->CreateTensorWithDataAsOrtValue(
        memory_info, speed_data, 4,
        speed_dims, speed_dims_len, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &speed_tensor));
    return speed_tensor;
}

int init_model(const char* model_path){

    // Initialize the ONNX Runtime API table
    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!g_ort) {
        MSG(2, "ERROR: Failed to initialize ONNX Runtime API.\n");
        return -1;
    }

    // Environment and Session Setup
    CHECK_STATUS(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "kitten_tts_inference", &env));

    CHECK_STATUS(g_ort->CreateSessionOptions(&session_options));
    // Default provider is CPU when no specific provider is added
    CHECK_STATUS(g_ort->CreateSession(env, model_path, session_options, &session));

    return 0;
}

void cleanup_model(){
    g_ort->ReleaseSession(session);
    g_ort->ReleaseSessionOptions(session_options);
    g_ort->ReleaseEnv(env);
}

// this is where the model is ran.
// the output is a sound wav in the form of a float array
GArray* run_model(GArray *inputs_array, GArray *styles_array, float speed){
    // Prepare Input Data & Allocators
    OrtMemoryInfo* memory_info = NULL;
    CHECK_STATUS(g_ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info));

    OrtValue* input_ids_tensor = create_inputs_tensor(memory_info, inputs_array);
    OrtValue* style_tensor = create_styles_tensor(memory_info, styles_array);
    float speed_data[] = {speed};
    OrtValue* speed_tensor = create_speed_tensor(memory_info, speed_data);

    // Group Input Names and Tensors
    const char* input_names[] = {"input_ids", "style", "speed"};
    const OrtValue* input_tensors[] = {input_ids_tensor, style_tensor, speed_tensor};

    // models output tensor.
    size_t output_count = 1;
    const char* output_names[] = {"waveform"}; // Match your specific model definition if needed
    OrtValue* output_tensor = NULL;

    // Run Inference
    CHECK_STATUS(g_ort->Run(
        session,
        NULL,
        input_names,
        input_tensors,
        3,             // Total number of input elements
        output_names,  // Array of target output layer names
        output_count,  // Number of target outputs
        &output_tensor // Target tensor
    ));

    // Get the Tensor Type and Shape Info
    OrtTensorTypeAndShapeInfo* tensor_info = NULL;
    CHECK_STATUS(g_ort->GetTensorTypeAndShape(output_tensor, &tensor_info));

    // Extract Total Element Count
    size_t total_elements = 0;
    CHECK_STATUS(g_ort->GetTensorShapeElementCount(tensor_info, &total_elements));

    // Access the Underlying Raw Data Buffer
    float* float_array = NULL;
    // float_array need not be freed its simply a pointer to onnx data, it will be
    // freed when output_tensor is.
    CHECK_STATUS(g_ort->GetTensorMutableData(output_tensor, (void**)&float_array));

    GArray *copy = g_array_sized_new(FALSE, FALSE, sizeof(float), total_elements);
    g_array_append_vals(copy, float_array, total_elements);

    // Free Allocated System Memory Structures
    g_ort->ReleaseTensorTypeAndShapeInfo(tensor_info);
    g_ort->ReleaseValue(input_ids_tensor);
    g_ort->ReleaseValue(style_tensor);
    g_ort->ReleaseValue(speed_tensor);
    g_ort->ReleaseValue(output_tensor);
    g_ort->ReleaseMemoryInfo(memory_info);

    return copy;
}

// speechd expects a wav of shorts but our model creates a wave of floats
// here we convert the floats wav to a shorts wav.
// based off my testing the floats wav values are always between -1 and 1
// thus we can convert to shorts using that range.
void convert_float_to_short(const float* in_buffer, GArray* out_buffer, size_t num_samples) {
    for (size_t i = 0; i < num_samples; i++) {
        // Scale standard floating point [-1.0, 1.0] to 16-bit range
        float scaled = in_buffer[i] * 32767.0f;

        // Clamp values to avoid nasty integer overflow wrap-around
        if (scaled > 32767.0f) {
            scaled = 32767.0f;
        } else if (scaled < -32768.0f) {
            scaled = -32768.0f;
        }

        // Cast to short
        g_array_index(out_buffer, gshort, i) = (short)scaled;
    }
}

/*
Here we put all the steps togeather.
The input is the text we wish to generate for.
The output is a GArray of shorts with our audio.
*/
GArray* kitten_speak(const char* data){
    // turn the text into phonemes
    GString *phonemes = get_phonemes(data);
    if (phonemes==NULL){
        return NULL;
    }
    const char* phonemes1 = phonemes->str;

    //The model needs the text to be converted to indices based off SYMBOLS
    GArray *inputs_array = get_char_indices(phonemes1, SYMBOLS);
    if (inputs_array==NULL){
        g_string_free(phonemes, TRUE);
        return NULL;
    }

    // we have to get the voice styles.
    GArray *styles_array = get_style(data, voice->str);
    if (styles_array==NULL){
        g_array_unref(inputs_array);
        g_string_free(phonemes, TRUE);
        return NULL;
    }

    // Run the model using onnx.
    GArray* output = run_model(inputs_array, styles_array, speed);

    // output comes out as an array of floats representing the wav.
    // but we need an array of shorts for speechd to use it.
    GArray *output_s = g_array_sized_new(TRUE, TRUE, sizeof(gshort), output->len);
    g_array_set_size(output_s, output->len);
    convert_float_to_short((float*)output->data, output_s, output->len);

    //clean up.
    g_array_unref(output);
    g_array_unref(inputs_array);
    g_array_unref(styles_array);
    g_string_free(phonemes, TRUE);

    return output_s;
}

// change the model and voice bin to the given model_filename and voice_filename.
int reload_models_and_voices(const char *model_filename, const char* voice_filename){
    //first clean up the old voice model and voice bin.
    cleanup_voice_style();
    cleanup_model();

    //change to the new paths
    char *tmp;
    tmp = g_build_filename(model_dir->str, model_filename, NULL);
    g_string_assign(model_path, tmp);
    g_free(tmp);

    tmp = g_build_filename(model_dir->str, voice_filename, NULL);
    g_string_assign(voices_path, tmp);
    g_free(tmp);

    //reload the model.
    init_voice_style(voices_path->str);
    init_model(model_path->str);
    return 0;
}