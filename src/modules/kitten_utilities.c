/*
The MIT License (MIT)

Copyright © 2026 John Settlemyer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "kitten.h"

/*
For reference, models+voices can be downloaded from these urls.
https://huggingface.co/KittenML/kitten-tts-micro-0.8/resolve/main/kitten_tts_micro_v0_8.onnx
https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_micro.bin
https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/kitten_tts_mini_v0_8.onnx
https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_mini.bin
https://huggingface.co/KittenML/kitten-tts-nano-0.2/resolve/main/kitten_tts_nano_v0_2.onnx
https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_nano.bin
*/

const FileInfo DEFAULT_FILES[] = {
    {
        "Normal",
        "kitten_tts_micro_v0_8.onnx",
        "95481626fee1ba70ce683e69c534fc7cb38433c46ce42d3abbeafb4b9f1a4123",
        "voices_micro.bin",
        "12ad10f1fcce8a458b5cf79769b8edd4ba0e11e9fb6532fd192c3500b2b37a5d"
    },
    {
        "High",
        "kitten_tts_mini_v0_8.onnx",
        "0f5bbae4fc4800c98dbc544a87ecfa79510de2fb8222db30d12e5bfe9177df91",
        "voices_mini.bin",
        "0e4965b46333db53ce09c73842623bf7055ea62c67f78803cb7ea9c16da6ac2b"
    },
    {
        "Low",
        "kitten_tts_nano_v0_2.onnx",
        "42fa8809db319cd7c4c83b3c501e2313bf90edf610235291cad605e4adcb242d",
        "voices_nano.bin",
        "42a40a24a352a38657d6cb86ceee51bbc2b7780b29e04fb60bcdf959adccea01"
    }
};

#define NUM_FILES (sizeof(DEFAULT_FILES) / sizeof(DEFAULT_FILES[0]))

GString *distro_target_subdir;
GHashTable *files_hash_table;

void file_hash_create_sub_table(GHashTable *ht, const char* quality){
	GHashTable *tmp = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        g_free,
        g_free
    );

	g_hash_table_insert(ht, g_strdup(quality), tmp);
}

void file_hash_add_sub_key(GHashTable *ht, const char* quality, const char* key, const char* value){
	GHashTable *sub = g_hash_table_lookup(ht, quality);
	g_hash_table_insert(sub, g_strdup(key), g_strdup(value));
}

char* file_hash_get_value(GHashTable *ht, const char* quality, const char* key){
	GHashTable *sub = g_hash_table_lookup(ht, quality);
	char* value = g_hash_table_lookup(sub, key);
	return value;
}

void build_hash_from_defaults(GHashTable *ht){
	for (size_t i = 0; i < NUM_FILES; i++) {
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "model_filename", DEFAULT_FILES[i].model_filename);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "model_sha256", DEFAULT_FILES[i].model_expected_sha256);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "voices_filename", DEFAULT_FILES[i].voice_filename);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "voices_sha256", DEFAULT_FILES[i].voice_expected_sha256);
	}
}

void init_file_hashtable_and_distro_subdir(void){

    distro_target_subdir = g_string_new("/usr/share/speech-dispatcher/models/kitten");

    files_hash_table = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        g_free,
        (GDestroyNotify) g_hash_table_destroy
    );

    file_hash_create_sub_table(files_hash_table, "Low");
    file_hash_create_sub_table(files_hash_table, "High");
    file_hash_create_sub_table(files_hash_table, "Normal");
}

// Helper function to recursively create directories
static int ensure_directory_exists(const char *path) {
    if (g_mkdir_with_parents(path, 0755) != 0) {
        fprintf(stderr, "Error: Failed to create directory '%s': %s\n", path, g_strerror(errno));
        return -1;
    }
    return 0;
}

// Helper function to check if file exists
static int file_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

// Verify file SHA256 using GLib GChecksum
static int verify_sha256(const char *filepath, const char *expected_sha256) {
    GMappedFile *mfile = g_mapped_file_new(filepath, FALSE, NULL);
    if (!mfile) {
        fprintf(stderr, "Error: Failed to memory-map file '%s' for SHA256 calculation.\n", filepath);
        return -1;
    }

    gsize length = g_mapped_file_get_length(mfile);
    const gchar *contents = g_mapped_file_get_contents(mfile);//contents must not be freed because it is owned by mfile.

    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)contents, length);
    const gchar *computed_sha256 = g_checksum_get_string(checksum);

    int match = (g_ascii_strcasecmp(computed_sha256, expected_sha256) == 0);

    if (!match) {
        fprintf(stderr, "Error: Checksum mismatch for '%s'!\n Expected: %s\n Computed: %s\n",
                filepath, expected_sha256, computed_sha256);
    }

    g_checksum_free(checksum);
    g_mapped_file_unref(mfile);

    return match ? 0 : -1;
}

bool file_integrity_check(char *target_dir, char* filename, char* sha){
    bool output = true;
    char *full_path = g_build_filename(target_dir, filename, NULL);
    // check if the file exists.
    if (!file_exists(full_path)) {
        output = false;
    } else {
        // verify the sha
        if (verify_sha256(full_path, sha) != 0) {
            fprintf(stderr, "Integrity check failed for '%s' in '%s'.\n", filename, full_path);
            output = false;
        }
    }

    g_free(full_path);
    return output;
}

bool verfiy_all_files(char *target_dir){
    bool ret=true;

    GHashTableIter iter;
    gpointer quality, value;

    g_hash_table_iter_init(&iter, files_hash_table);
    while (g_hash_table_iter_next(&iter, &quality, &value)) {
        char *model_filename = file_hash_get_value(files_hash_table, quality, "model_filename");
        char *model_sha = file_hash_get_value(files_hash_table, quality, "model_sha256");
        if (!file_integrity_check(target_dir,model_filename,model_sha)){
            ret = false;
        }

        char* voice_filename = file_hash_get_value(files_hash_table, quality, "voices_filename");
        char* voice_sha = file_hash_get_value(files_hash_table, quality, "voices_sha256");
        if (!file_integrity_check(target_dir,voice_filename,voice_sha)){
            ret = false;
        }
    }

    return ret;
}

/*
determine if we should use distro_target_subdir or TARGET_SUBDIR when looking
for models. The rules are to use the distro path if all the files
are in that path, and pass the verify_sha256. If not then fall back to TARGET_SUBDIR
and download the files if needed.
we should only check this once even if its called again.
*/
bool use_distro_path_cache = false;
bool use_distro_path_return = false;
bool use_distro_path(void){
    if (use_distro_path_cache){
        return use_distro_path_return;
    }
    char *target_dir_distro = distro_target_subdir->str;
    bool use_distro_dir=true;

    use_distro_dir = verfiy_all_files(target_dir_distro);

    use_distro_path_cache = true;
    use_distro_path_return = use_distro_dir;
    return use_distro_dir;
}

int verify_user_models(void) {

    char *target_dir;
    int overall_status = 0;

    if (!use_distro_path()){
        // Build absolute destination directory path: ~/.cache/speech-dispatcher/kitten
        target_dir = g_build_filename(home_dir, TARGET_SUBDIR, NULL);
    } else {
        return 0; // Using distro path nothing else needs to be done.
    }

    if (ensure_directory_exists(target_dir) != 0) {
        overall_status = -1;
    }

    if (!verfiy_all_files(target_dir)){
        overall_status = -1;
    }

    // Cleanup resources
    g_free(target_dir);
    return overall_status;
}

int init_paths(void){

    home_dir = g_get_home_dir();
    if (!home_dir) {
        fprintf(stderr, "Error: Could not determine home directory.\n");
        return -1;
    }

    if (use_distro_path()){
        // use disto path.
        fprintf(stderr, "INFO: using distro path for loading models.\n");
        model_dir = g_string_new(distro_target_subdir->str);
    } else {
        // Build absolute destination directory path: ~/.cache/speech-dispatcher/kitten
        fprintf(stderr, "INFO: using user path for loading models.\n");
        model_dir = g_string_new_take(g_build_filename(home_dir, TARGET_SUBDIR, NULL));
    }

    char *model_filename = file_hash_get_value(files_hash_table, "Normal", "model_filename");
    char *voice_filename = file_hash_get_value(files_hash_table, "Normal", "voices_filename");
    model_path = g_string_new_take(g_build_filename(model_dir->str, model_filename, NULL));
    voices_path = g_string_new_take(g_build_filename(model_dir->str, voice_filename, NULL));

    // verify the user models.
    if (verify_user_models() == -1){
        fprintf(stderr, "Error: Verification of models failed\n");
        return -1;
    };

    return 0;
}