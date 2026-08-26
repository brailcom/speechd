/*
The MIT License (MIT)

Copyright © 2026 John Settlemyer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "kitten.h"

const FileInfo DEFAULT_FILES[] = {
    {
        "Normal",
        "https://huggingface.co/KittenML/kitten-tts-micro-0.8/resolve/main/kitten_tts_micro_v0_8.onnx",
        "kitten_tts_micro_v0_8.onnx",
        "41384970",
        "95481626fee1ba70ce683e69c534fc7cb38433c46ce42d3abbeafb4b9f1a4123",
        "https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_micro.bin",
        "voices_micro.bin",
        "3276800",
        "12ad10f1fcce8a458b5cf79769b8edd4ba0e11e9fb6532fd192c3500b2b37a5d"
    },
    {
        "High",
        "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/kitten_tts_mini_v0_8.onnx",
        "kitten_tts_mini_v0_8.onnx",
        "78268016",
        "0f5bbae4fc4800c98dbc544a87ecfa79510de2fb8222db30d12e5bfe9177df91",
        "https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_mini.bin",
        "voices_mini.bin",
        "3276800",
        "0e4965b46333db53ce09c73842623bf7055ea62c67f78803cb7ea9c16da6ac2b"
    },
    {
        "Low",
        "https://huggingface.co/KittenML/kitten-tts-nano-0.2/resolve/main/kitten_tts_nano_v0_2.onnx",
        "kitten_tts_nano_v0_2.onnx",
        "23804156",
        "42fa8809db319cd7c4c83b3c501e2313bf90edf610235291cad605e4adcb242d",
        "https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_nano.bin",
        "voices_nano.bin",
        "8192",
        "42a40a24a352a38657d6cb86ceee51bbc2b7780b29e04fb60bcdf959adccea01"
    }
};

#define NUM_FILES (sizeof(DEFAULT_FILES) / sizeof(DEFAULT_FILES[0]))

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

int file_hash_get_value_as_num(GHashTable *ht, const char* quality, const char* key){
	GHashTable *sub = g_hash_table_lookup(ht, quality);
	char* value = g_hash_table_lookup(sub, key);
	gchar *endptr;
	int num = g_ascii_strtoll(value, &endptr, 10); // convert str to int
	return num;
}

char* file_hash_get_value(GHashTable *ht, const char* quality, const char* key){
	GHashTable *sub = g_hash_table_lookup(ht, quality);
	char* value = g_hash_table_lookup(sub, key);
	return value;
}

void build_hash_from_defaults(GHashTable *ht){
	for (size_t i = 0; i < NUM_FILES; i++) {
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "model_url", DEFAULT_FILES[i].model_url);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "model_filename", DEFAULT_FILES[i].model_filename);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "model_size", DEFAULT_FILES[i].model_expected_size);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "model_sha256", DEFAULT_FILES[i].model_expected_sha256);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "voices_url", DEFAULT_FILES[i].voice_url);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "voices_filename", DEFAULT_FILES[i].voice_filename);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "voices_size", DEFAULT_FILES[i].voice_expected_size);
		file_hash_add_sub_key(ht, DEFAULT_FILES[i].quality, "voices_sha256", DEFAULT_FILES[i].voice_expected_sha256);
	}
}

void init_file_hashtable(void){
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

// Callback for curl to write HTTP data to disk
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

// Download file with libcurl
static int download_file(CURL *curl, const char *url, const char *dest_path) {
    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open destination file '%s' for writing: %s\n", dest_path, g_strerror(errno));
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);     // Fail on HTTP errors (>=400)

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);

    if (res != CURLE_OK) {
        fprintf(stderr, "Error: Download failed for '%s': %s\n", url, curl_easy_strerror(res));
        remove(dest_path); // Clean up partial download
        return -1;
    }

    return 0;
}

// Verify file SHA256 using GLib GChecksum
static int verify_sha256(const char *filepath, const char *expected_sha256) {
    GMappedFile *mfile = g_mapped_file_new(filepath, FALSE, NULL);
    if (!mfile) {
        fprintf(stderr, "Error: Failed to memory-map file '%s' for SHA256 calculation.\n", filepath);
        return -1;
    }

    gsize length = g_mapped_file_get_length(mfile);
    const gchar *contents = g_mapped_file_get_contents(mfile);

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

bool distro_file_integrity_check(char *target_dir_distro, char* filename, char* sha){
    bool output = true;
    char *full_path = g_build_filename(target_dir_distro, filename, NULL);
    // check if the file exists.
    if (!file_exists(full_path)) {
        output = false;
    } else {
        // verify the sha
        if (verify_sha256(full_path, sha) != 0) {
            fprintf(stderr, "Integrity check failed for '%s' in '%s'.\n", filename, full_path);
            fprintf(stderr, "Cannot use distro directory\n");
            output = false;
        }
    }

    g_free(full_path);
    return output;
}

// determine if we should use DISTRO_TARGET_SUBDIR or TARGET_SUBDIR when looking
// for models. The rules are to use the distro path if all the files
// are in that path, and pass the verify_sha256. If not then fall back to TARGET_SUBDIR
// and download the files if needed.
// we should only check this once even if its called again.
bool use_distro_path_cache = false;
bool use_distro_path_return = false;
bool use_distro_path(void){
    if (use_distro_path_cache){
        return use_distro_path_return;
    }
    char *target_dir_distro = DISTRO_TARGET_SUBDIR;
    bool use_distro_dir=true;

    GHashTableIter iter;
    gpointer quality, value;

    g_hash_table_iter_init(&iter, files_hash_table);
    while (g_hash_table_iter_next(&iter, &quality, &value)) {
        char *model_filename = file_hash_get_value(files_hash_table, quality, "model_filename");
        char *model_sha = file_hash_get_value(files_hash_table, quality, "model_sha256");
        if (!distro_file_integrity_check(target_dir_distro,model_filename,model_sha)){
            use_distro_dir = false;
        }

        char* voice_filename = file_hash_get_value(files_hash_table, quality, "voices_filename");
        char* voice_sha = file_hash_get_value(files_hash_table, quality, "voices_sha256");
        if (!distro_file_integrity_check(target_dir_distro, voice_filename,voice_sha)){
            use_distro_dir = false;
        }
    }

    use_distro_path_cache = true;
    use_distro_path_return = use_distro_dir;
    return use_distro_dir;
}

int download_and_verify(char *target_dir, CURL *curl, char* url, char* filename, char* sha, int size){
    char *full_path = g_build_filename(target_dir, filename, NULL);

    // Check if file already exists
    if (file_exists(full_path)) {

        // Verify SHA256 even for files that have already been downloaded.
        if (verify_sha256(full_path, sha) != 0) {
            fprintf(stderr, "Error: Integrity check failed for '%s'.\n", filename);
            g_free(full_path);
            return -1;
        } else {
            fprintf(stderr, "Info: File '%s' already exists. Skipping download.\n", filename);
            g_free(full_path);
            return 0;
        }
    }

    fprintf(stderr, "Info: Downloading '%s'...\n", filename);
    if (download_file(curl, url, full_path) != 0) {
        fprintf(stderr, "Error: Aborting process due to download error.\n");
        g_free(full_path);
        return -1;
    }

    // Verify File Size
    struct stat st;
    if (stat(full_path, &st) != 0) {
        fprintf(stderr, "Error: Could not stat downloaded file '%s'.\n", full_path);
        g_free(full_path);
        return -1;
    }

    if ((curl_off_t)st.st_size != (curl_off_t)size) {
        fprintf(stderr, "Error: File size mismatch for '%s'! Expected: %ld bytes, Got: %ld bytes.\n",
                filename, (long)size, (long)st.st_size);
        g_free(full_path);
        return -1;
    }

    // Verify SHA256 using GLib
    if (verify_sha256(full_path, sha) != 0) {
        fprintf(stderr, "Error: Integrity check failed for '%s'.\n", filename);
        g_free(full_path);
        return -1;
    }

    fprintf(stderr, "Info: Successfully downloaded and verified '%s'.\n", filename);
    g_free(full_path);
    return 0;
}

// downloads the models and voices if they do not already exist.
// also verifys using sha256 and checks file size.
int download_models(void) {

    char *target_dir;
    if (!use_distro_path()){
        // Build absolute destination directory path: ~/.cache/speech-dispatcher/kitten
        target_dir = g_build_filename(home_dir, TARGET_SUBDIR, NULL);
    } else {
        return 0; //no need to download if the models are already in the distro path.
    }

    if (ensure_directory_exists(target_dir) != 0) {
        g_free(target_dir);
        return -1;
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        fprintf(stderr, "Error: Failed to initialize libcurl.\n");
        g_free(target_dir);
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Failed to create libcurl handle.\n");
        curl_global_cleanup();
        g_free(target_dir);
        return -1;
    }

    int overall_status = 0;

    GHashTableIter iter;
    gpointer quality, value;

    g_hash_table_iter_init(&iter, files_hash_table);
    while (g_hash_table_iter_next(&iter, &quality, &value)) {
        char *model_url = file_hash_get_value(files_hash_table, quality, "model_url");
        char *model_filename = file_hash_get_value(files_hash_table, quality, "model_filename");
        char *model_sha = file_hash_get_value(files_hash_table, quality, "model_sha256");
        int model_size = file_hash_get_value_as_num(files_hash_table, quality, "model_size");

        if (download_and_verify(target_dir, curl, model_url, model_filename, model_sha, model_size) == -1){
            overall_status = -1;
            break;
        }

        char* voice_filename = file_hash_get_value(files_hash_table, quality, "voices_filename");
        char* voice_sha = file_hash_get_value(files_hash_table, quality, "voices_sha256");
        char* voice_url = file_hash_get_value(files_hash_table, quality, "voices_url");
        int voice_size = file_hash_get_value_as_num(files_hash_table, quality, "voices_size");

        if (download_and_verify(target_dir, curl, voice_url, voice_filename, voice_sha, voice_size) == -1){
            overall_status = -1;
            break;
        }
    }

    // Cleanup resources
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    g_free(target_dir);

    return overall_status;
}