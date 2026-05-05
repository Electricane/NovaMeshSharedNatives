#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define BUF_SIZE 16384
#define VALUE_SIZE 4096

static int read_file(const char *path, char *out, size_t out_size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    size_t n = fread(out, 1, out_size - 1, f);
    out[n] = '\0';
    fclose(f);

    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == ' ' || out[n - 1] == '\t')) {
        out[--n] = '\0';
    }

    return 1;
}

static int read_first_available(const char **paths, char *out, size_t out_size) {
    for (int i = 0; paths[i] != NULL; i++) {
        if (read_file(paths[i], out, out_size)) {
            return 1;
        }
    }
    return 0;
}

static void json_escape(const char *value, char *escaped, size_t escaped_size) {
    size_t j = 0;

    for (size_t i = 0; value[i] != '\0' && j < escaped_size - 2; i++) {
        char c = value[i];

        if (c == '\n') {
            escaped[j++] = '\\';
            escaped[j++] = 'n';
        } else if (c == '\r') {
            escaped[j++] = '\\';
            escaped[j++] = 'r';
        } else if (c == '\t') {
            escaped[j++] = '\\';
            escaped[j++] = 't';
        } else if (c == '"') {
            escaped[j++] = '\\';
            escaped[j++] = '"';
        } else if (c == '\\') {
            escaped[j++] = '\\';
            escaped[j++] = '\\';
        } else {
            escaped[j++] = c;
        }
    }

    escaped[j] = '\0';
}

static void add_string_field(char *json, size_t size, const char *key, const char *value, int *first) {
    if (!value || value[0] == '\0') return;

    char escaped[VALUE_SIZE];
    json_escape(value, escaped, sizeof(escaped));

    snprintf(
        json + strlen(json),
        size - strlen(json),
        "%s\"%s\":\"%s\"",
        *first ? "" : ",",
        key,
        escaped
    );

    *first = 0;
}

static void add_number_field(char *json, size_t size, const char *key, long long value, int *first) {
    snprintf(
        json + strlen(json),
        size - strlen(json),
        "%s\"%s\":%lld",
        *first ? "" : ",",
        key,
        value
    );

    *first = 0;
}

static void add_double_field(char *json, size_t size, const char *key, double value, int *first) {
    snprintf(
        json + strlen(json),
        size - strlen(json),
        "%s\"%s\":%.2f",
        *first ? "" : ",",
        key,
        value
    );

    *first = 0;
}

static int parse_gpu_load(const char *load, long long *usage_percent, long long *freq_hz) {
    if (!load || load[0] == '\0') return 0;

    long long usage = -1;
    long long freq = -1;

    if (sscanf(load, "%lld@%lldHz", &usage, &freq) == 2) {
        *usage_percent = usage;
        *freq_hz = freq;
        return 1;
    }

    char *end = NULL;
    usage = strtoll(load, &end, 10);
    if (end != load) {
        *usage_percent = usage;
        *freq_hz = -1;
        return 1;
    }

    return 0;
}

static double estimate_gpu_activity_score(long long usage_percent, long long cur_freq, long long max_freq) {
    if (usage_percent < 0) return -1.0;
    if (cur_freq <= 0 || max_freq <= 0) return (double) usage_percent;

    return ((double) usage_percent) * ((double) cur_freq / (double) max_freq);
}

static long long parse_ll(const char *s) {
    if (!s || s[0] == '\0') return -1;
    return strtoll(s, NULL, 10);
}

JNIEXPORT jstring JNICALL Java_novamesh_MaliGpuStats_getGpuStatsJsonNative
  (JNIEnv *env, jclass cls) {

    char json[BUF_SIZE];
    char value[VALUE_SIZE];
    int first = 0;

    strcpy(json, "{\"available\":true");

    const char *cur_freq_paths[] = {
        "/sys/class/devfreq/fde60000.gpu/cur_freq",
        "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/cur_freq",
        NULL
    };

    const char *min_freq_paths[] = {
        "/sys/class/devfreq/fde60000.gpu/min_freq",
        "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/min_freq",
        NULL
    };

    const char *max_freq_paths[] = {
        "/sys/class/devfreq/fde60000.gpu/max_freq",
        "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/max_freq",
        NULL
    };

    const char *available_freqs_paths[] = {
        "/sys/class/devfreq/fde60000.gpu/available_frequencies",
        "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/available_frequencies",
        NULL
    };

    const char *governor_paths[] = {
        "/sys/class/devfreq/fde60000.gpu/governor",
        "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/governor",
        NULL
    };

    const char *load_paths[] = {
        "/sys/class/devfreq/fde60000.gpu/load",
        "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/load",
        NULL
    };

    const char *trans_stat_paths[] = {
        "/sys/class/devfreq/fde60000.gpu/trans_stat",
        "/sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/trans_stat",
        NULL
    };

    const char *mali_util_paths[] = {
        "/sys/kernel/debug/mali0/gpu_utilization",
        NULL
    };

    const char *mali_mem_paths[] = {
        "/sys/kernel/debug/mali0/mem_pool_size",
        NULL
    };

    const char *mali_pm_paths[] = {
        "/sys/kernel/debug/mali0/pm_status",
        NULL
    };

    const char *panfrost_gpu_paths[] = {
        "/sys/kernel/debug/dri/0/panfrost/gpu",
        NULL
    };

    long long cur_freq = -1;
    long long max_freq = -1;
    long long usage_percent = -1;
    long long load_freq_hz = -1;

    if (read_first_available(cur_freq_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "gpu_cur_freq", value, &first);
        cur_freq = parse_ll(value);
    }

    if (read_first_available(min_freq_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "gpu_min_freq", value, &first);
    }

    if (read_first_available(max_freq_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "gpu_max_freq", value, &first);
        max_freq = parse_ll(value);
    }

    if (read_first_available(available_freqs_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "gpu_available_freqs", value, &first);
    }

    if (read_first_available(governor_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "gpu_governor", value, &first);
    }

    if (read_first_available(load_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "gpu_load", value, &first);

        if (parse_gpu_load(value, &usage_percent, &load_freq_hz)) {
            add_number_field(json, sizeof(json), "gpu_load_percent", usage_percent, &first);

            if (load_freq_hz >= 0) {
                add_number_field(json, sizeof(json), "gpu_load_freq_hz", load_freq_hz, &first);
            }
        }
    }

    if (usage_percent >= 0) {
        double score = estimate_gpu_activity_score(usage_percent, cur_freq, max_freq);
        if (score >= 0.0) {
            add_double_field(json, sizeof(json), "gpu_activity_score", score, &first);
        }
    }

    if (read_first_available(trans_stat_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "gpu_trans_stat", value, &first);
    }

    if (read_first_available(mali_util_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "mali_utilization", value, &first);
    }

    if (read_first_available(mali_mem_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "mali_memory", value, &first);
    }

    if (read_first_available(mali_pm_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "mali_pm_status", value, &first);
    }

    if (read_first_available(panfrost_gpu_paths, value, sizeof(value))) {
        add_string_field(json, sizeof(json), "panfrost_gpu", value, &first);
    }

    strcat(json, "}");

    return (*env)->NewStringUTF(env, json);
}