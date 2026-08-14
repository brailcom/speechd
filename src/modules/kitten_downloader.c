#include "kitten.h"

const FileInfo FILES[] = {
    {
        "https://huggingface.co/KittenML/kitten-tts-micro-0.8/resolve/main/kitten_tts_micro_v0_8.onnx",
        "kitten_tts_micro_v0_8.onnx",
        41384970,
        "95481626fee1ba70ce683e69c534fc7cb38433c46ce42d3abbeafb4b9f1a4123"
    },
    {
        "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/kitten_tts_mini_v0_8.onnx",
        "kitten_tts_mini_v0_8.onnx",
        78268016,
        "0f5bbae4fc4800c98dbc544a87ecfa79510de2fb8222db30d12e5bfe9177df91"
    },
    {
        "https://huggingface.co/KittenML/kitten-tts-nano-0.2/resolve/main/kitten_tts_nano_v0_2.onnx",
        "kitten_tts_nano_v0_2.onnx",
        23804156,
        "42fa8809db319cd7c4c83b3c501e2313bf90edf610235291cad605e4adcb242d"
    },
    {
        "https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_micro.bin",
        "voices_micro.bin",
        3276800,
        "12ad10f1fcce8a458b5cf79769b8edd4ba0e11e9fb6532fd192c3500b2b37a5d"
    },
    {
        "https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_mini.bin",
        "voices_mini.bin",
        3276800,
        "0e4965b46333db53ce09c73842623bf7055ea62c67f78803cb7ea9c16da6ac2b"
    },
    {
        "https://github.com/jsett/kittenvoices/raw/refs/heads/main/voices_nano.bin",
        "voices_nano.bin",
        8192,
        "42a40a24a352a38657d6cb86ceee51bbc2b7780b29e04fb60bcdf959adccea01"
    }
};

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

// downloads the models and voices if they do not already exist.
// also verifys using sha256 and checks file size.
int download_models(void) {
    // Build absolute destination directory path: ~/.config/speech-dispatcher/extra/
    char *target_dir = g_build_filename(home_dir, TARGET_SUBDIR, NULL);

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

    for (size_t i = 0; i < NUM_FILES; i++) {
        char *full_path = g_build_filename(target_dir, FILES[i].filename, NULL);

        // Check if file already exists
        if (file_exists(full_path)) {

            // Verify SHA256 even for files that have already been downloaded.
            if (verify_sha256(full_path, FILES[i].expected_sha256) != 0) {
                fprintf(stderr, "Error: Integrity check failed for '%s'.\n", FILES[i].filename);
                g_free(full_path);
                overall_status = -1;
                break;
            } else {
                fprintf(stderr, "Info: File '%s' already exists. Skipping download.\n", FILES[i].filename);
                g_free(full_path);
                continue;
            }
        }

        fprintf(stderr, "Info: Downloading '%s'...\n", FILES[i].filename);
        if (download_file(curl, FILES[i].url, full_path) != 0) {
            fprintf(stderr, "Error: Aborting process due to download error.\n");
            g_free(full_path);
            overall_status = -1;
            break;
        }

        // Verify File Size
        struct stat st;
        if (stat(full_path, &st) != 0) {
            fprintf(stderr, "Error: Could not stat downloaded file '%s'.\n", full_path);
            g_free(full_path);
            overall_status = -1;
            break;
        }

        if ((curl_off_t)st.st_size != FILES[i].expected_size) {
            fprintf(stderr, "Error: File size mismatch for '%s'! Expected: %ld bytes, Got: %ld bytes.\n",
                    FILES[i].filename, (long)FILES[i].expected_size, (long)st.st_size);
            g_free(full_path);
            overall_status = -1;
            break;
        }

        // Verify SHA256 using GLib
        if (verify_sha256(full_path, FILES[i].expected_sha256) != 0) {
            fprintf(stderr, "Error: Integrity check failed for '%s'.\n", FILES[i].filename);
            g_free(full_path);
            overall_status = -1;
            break;
        }

        fprintf(stderr, "Info: Successfully downloaded and verified '%s'.\n", FILES[i].filename);
        g_free(full_path);
    }

    // Cleanup resources
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    g_free(target_dir);

    return overall_status;
}