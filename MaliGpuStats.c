#include <jni.h>
#include <stdio.h>
#include <string.h>
#include "MaliGpuStats.h"

#define BUF_SIZE 8192

static int read_file(const char *path, char *out, size_t out_size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    size_t n = fread(out, 1, out_size - 1, f);
    out[n] = '\0';
    fclose(f);

    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == ' ')) {
        out[--n] = '\0';
    }

    return 1;
}

static void add_field(char *json, size_t size, const char *key, const char *value, int *first) {
    if (!value || strlen(value) == 0) return;

    snprintf(
        json + strlen(json),
        size - strlen(json),
        "%s\"%s\":\"%s\"",
        *first ? "" : ",",
        key,
        value
    );

    *first = 0;
}

JNIEXPORT jstring JNICALL Java_MaliGpuStats_getGpuStatsJsonNative
  (JNIEnv *env, jclass cls) {

    char json[BUF_SIZE];
    char value[1024];
    int first = 1;

    json[0] = '\0';
    strcat(json, "{\"available\":true");

    first = 0;

    const char *paths[][2] = {
        {"gpu_cur_freq", "/sys/class/devfreq/fde60000.gpu/cur_freq"},
        {"gpu_min_freq", "/sys/class/devfreq/fde60000.gpu/min_freq"},
        {"gpu_max_freq", "/sys/class/devfreq/fde60000.gpu/max_freq"},
        {"gpu_available_freqs", "/sys/class/devfreq/fde60000.gpu/available_frequencies"},
        {"gpu_governor", "/sys/class/devfreq/fde60000.gpu/governor"},
        {"gpu_load", "/sys/class/devfreq/fde60000.gpu/load"},

        {"gpu_cur_freq_alt", "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/cur_freq"},
        {"gpu_available_freqs_alt", "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/available_frequencies"},
        {"gpu_governor_alt", "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/governor"},

        {"mali_utilization", "/sys/kernel/debug/mali0/gpu_utilization"},
        {"mali_memory", "/sys/kernel/debug/mali0/mem_pool_size"},

        {NULL, NULL}
    };

    for (int i = 0; paths[i][0] != NULL; i++) {
        if (read_file(paths[i][1], value, sizeof(value))) {
            add_field(json, sizeof(json), paths[i][0], value, &first);
        }
    }

    strcat(json, "}");

    return (*env)->NewStringUTF(env, json);
}