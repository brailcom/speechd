#include <glib/gstdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <glib.h>
#include <onnxruntime_c_api.h>
#include <inttypes.h>
#include <sndfile.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <espeak-ng/espeak_ng.h>
#include <espeak-ng/speak_lib.h>
#include "module_utils.h"

#include <speechd_types.h>
#include "spd_module_main.h"

#define CHECK_STATUS(expr) \
    do { \
        OrtStatus* status = (expr); \
        if (status != NULL) { \
            const char* msg = g_ort->GetErrorMessage(status); \
            fprintf(stderr, "ONNX Runtime Error: %s\n", msg); \
            g_ort->ReleaseStatus(status); \
            exit(1); \
        } \
    } while (0)

extern int model_type;
extern int ROWS;
#define COLS 256

#define PAD "$"
#define PUNCTUATION ";:,.!?¡¿—…\"«»\"\" "
#define LETTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define LETTERS_IPA "ɑɐɒæɓʙβɔɕçɗɖðʤəɘɚɛɜɝɞɟʄɡɠɢʛɦɧħɥʜɨɪʝɭɬɫɮʟɱɯɰŋɳɲɴøɵɸθœɶʘɹɺɾɻʀʁɽʂʃʈʧʉʊʋⱱʌɣɤʍχʎʏʑʐʒʔʡʕʢǀǁǂǃˈˌːˑʼʴʰʱʲʷˠˤ˞↓↑→↗↘'̩'ᵻ"

#define SYMBOLS PAD PUNCTUATION LETTERS LETTERS_IPA

// setting var's
extern float speed;
//['Leo','Kiki','Hugo','Rosie','Bruno','Luna','Jasper','Bella']
extern GString *voice;
extern GString *voice_setting;
// paths var's
extern GString *model_path;
extern GString *voices_path;
extern const char *home_dir;
extern GString *model_dir;

extern bool stop_generation;

// holds and array of values that must be passed to the model based off the requested voice and length of the text.
extern float *voice_styles;

extern const OrtApi* g_ort;
extern OrtEnv* env;
extern OrtSessionOptions* session_options;
extern OrtSession* session;

#define TARGET_SUBDIR ".cache/speech-dispatcher/kitten"

typedef struct {
    const char *url;
    const char *filename;
    curl_off_t expected_size;
    const char *expected_sha256;
} FileInfo;

extern const FileInfo FILES[];

#define NUM_FILES (sizeof(FILES) / sizeof(FILES[0]))

#define VOICE_LIST(X) \
    X(Leo)            \
    X(Kiki)           \
    X(Hugo)           \
    X(Rosie)          \
    X(Bruno)          \
    X(Luna)           \
    X(Jasper)         \
    X(Bella)          \
    X(Leo_Low)        \
    X(Kiki_Low)       \
    X(Hugo_Low)       \
    X(Rosie_Low)      \
    X(Bruno_Low)      \
    X(Luna_Low)       \
    X(Jasper_Low)     \
    X(Bella_Low)      \
    X(Leo_High)       \
    X(Kiki_High)      \
    X(Hugo_High)      \
    X(Rosie_High)     \
    X(Bruno_High)     \
    X(Luna_High)      \
    X(Jasper_High)    \
    X(Bella_High)

#define DEFINE_VOICE(name_token) static SPDVoice voice_##name_token = { .name = #name_token, .language = "en" };
#define VOICE_PTR_ITEM(name_token) &voice_##name_token,

// kitten_downloader.c
int download_models(void);

// kitten_model.c
int init_voice_style(const char* voices_path);
void cleanup_voice_style();
GArray *get_style(const char *text, const char *voice);
GArray *get_char_indices(const gchar *locate, const gchar *index_str);
GString *get_phonemes(const char *text);
int init_model(const char* model_path);
void cleanup_model();
GArray* run_model(GArray *inputs_array, GArray *styles_array, float speed);
void convert_float_to_short(const float* in_buffer, GArray* out_buffer, size_t num_samples);
GArray* kitten_speak(const char* data);
int reload_models_and_voices(const char *model_filename, const char* voice_filename);

// kitten_worker.c
int init_model_thread_pool();
int cleanup_threads();
int model_change_voice(const char *var, const char *val);
int model_change_speed(const char *var, const char *val);
int add_generate_speech_task(const char* data, size_t bytes);
int model_stop_generation();