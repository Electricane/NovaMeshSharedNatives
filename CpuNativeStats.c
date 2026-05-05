#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE 65536
#define VALUE_SIZE 8192

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

static void json_escape(const char *value, char *escaped, size_t escaped_size) {
    size_t j = 0;

    for (size_t i = 0; value && value[i] != '\0' && j < escaped_size - 2; i++) {
        char c = value[i];

        if (c == '\n') {
            escaped[j++] = '\\'; escaped[j++] = 'n';
        } else if (c == '\r') {
            escaped[j++] = '\\'; escaped[j++] = 'r';
        } else if (c == '\t') {
            escaped[j++] = '\\'; escaped[j++] = 't';
        } else if (c == '"') {
            escaped[j++] = '\\'; escaped[j++] = '"';
        } else if (c == '\\') {
            escaped[j++] = '\\'; escaped[j++] = '\\';
        } else {
            escaped[j++] = c;
        }
    }

    escaped[j] = '\0';
}

static void add_string(char *json, size_t size, const char *key, const char *value, int *first) {
    if (!value || value[0] == '\0') return;

    char escaped[VALUE_SIZE];
    json_escape(value, escaped, sizeof(escaped));

    snprintf(json + strlen(json), size - strlen(json),
             "%s\"%s\":\"%s\"", *first ? "" : ",", key, escaped);

    *first = 0;
}

static void add_ll(char *json, size_t size, const char *key, long long value, int *first) {
    snprintf(json + strlen(json), size - strlen(json),
             "%s\"%s\":%lld", *first ? "" : ",", key, value);

    *first = 0;
}

static void add_double(char *json, size_t size, const char *key, double value, int *first) {
    snprintf(json + strlen(json), size - strlen(json),
             "%s\"%s\":%.2f", *first ? "" : ",", key, value);

    *first = 0;
}

static long long parse_ll_file(const char *path) {
    char buf[128];
    if (!read_file(path, buf, sizeof(buf))) return -1;
    return strtoll(buf, NULL, 10);
}

static void add_loadavg(char *json, size_t size, int *first) {
    char buf[256];
    if (read_file("/proc/loadavg", buf, sizeof(buf))) {
        add_string(json, size, "loadavg_raw", buf, first);
    }
}

static void add_uptime(char *json, size_t size, int *first) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return;

    double uptime = 0.0;
    double idle = 0.0;

    if (fscanf(f, "%lf %lf", &uptime, &idle) == 2) {
        add_double(json, size, "uptime_seconds", uptime, first);
        add_double(json, size, "idle_seconds_total", idle, first);
    }

    fclose(f);
}

static void add_cpu_stat_summary(char *json, size_t size, int *first) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;

    char cpu[16];
    long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
    long long irq = 0, softirq = 0, steal = 0, guest = 0, guest_nice = 0;

    int n = fscanf(
            f,
            "%15s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
            cpu,
            &user,
            &nice,
            &system,
            &idle,
            &iowait,
            &irq,
            &softirq,
            &steal,
            &guest,
            &guest_nice
    );

    fclose(f);

    if (n < 8) return;

    long long active = user + nice + system + irq + softirq + steal;
    long long idle_total = idle + iowait;
    long long total = active + idle_total;

    add_ll(json, size, "jiffies_user", user, first);
    add_ll(json, size, "jiffies_nice", nice, first);
    add_ll(json, size, "jiffies_system", system, first);
    add_ll(json, size, "jiffies_idle", idle, first);
    add_ll(json, size, "jiffies_iowait", iowait, first);
    add_ll(json, size, "jiffies_irq", irq, first);
    add_ll(json, size, "jiffies_softirq", softirq, first);
    add_ll(json, size, "jiffies_steal", steal, first);
    add_ll(json, size, "jiffies_active", active, first);
    add_ll(json, size, "jiffies_total", total, first);
}

static void add_context_switches(char *json, size_t size, int *first) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;

    char line[256];

    while (fgets(line, sizeof(line), f)) {
        char key[64];
        long long value;

        if (sscanf(line, "%63s %lld", key, &value) == 2) {
            if (strcmp(key, "ctxt") == 0) {
                add_ll(json, size, "context_switches", value, first);
            } else if (strcmp(key, "processes") == 0) {
                add_ll(json, size, "processes_created", value, first);
            } else if (strcmp(key, "procs_running") == 0) {
                add_ll(json, size, "processes_running", value, first);
            } else if (strcmp(key, "procs_blocked") == 0) {
                add_ll(json, size, "processes_blocked", value, first);
            }
        }
    }

    fclose(f);
}

static void add_thermal_info(char *json, size_t size, int *first) {
    char buf[256];

    if (read_file("/sys/class/thermal/thermal_zone0/temp", buf, sizeof(buf))) {
        long long milli = strtoll(buf, NULL, 10);
        add_double(json, size, "thermal_zone0_c", ((double)milli) / 1000.0, first);
    }

    if (read_file("/sys/devices/platform/soc/soc:firmware/get_throttled", buf, sizeof(buf))) {
        add_string(json, size, "throttled_raw", buf, first);
    }
}

static void add_per_core_native(char *json, size_t size, int *first) {
    long cores = sysconf(_SC_NPROCESSORS_CONF);
    if (cores <= 0) return;

    snprintf(json + strlen(json), size - strlen(json),
             "%s\"per_core_native\":[", *first ? "" : ",");

    *first = 0;

    for (long i = 0; i < cores; i++) {
        if (i > 0) {
            strncat(json, ",", size - strlen(json) - 1);
        }

        char path[256];
        char governor[128] = "";

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cpufreq/scaling_cur_freq", i);
        long long cur = parse_ll_file(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cpufreq/scaling_min_freq", i);
        long long min = parse_ll_file(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cpufreq/scaling_max_freq", i);
        long long max = parse_ll_file(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cpufreq/scaling_governor", i);
        read_file(path, governor, sizeof(governor));

        char escaped_governor[256];
        json_escape(governor, escaped_governor, sizeof(escaped_governor));

        snprintf(
                json + strlen(json),
                size - strlen(json),
                "{\"core\":%ld,\"cur_freq_khz\":%lld,\"min_freq_khz\":%lld,\"max_freq_khz\":%lld,\"governor\":\"%s\"}",
                i,
                cur,
                min,
                max,
                escaped_governor
        );
    }

    strncat(json, "]", size - strlen(json) - 1);
}

JNIEXPORT jstring JNICALL Java_novamesh_CpuNativeStats_getCpuNativeStatsJsonNative
  (JNIEnv *env, jclass cls) {

    char json[BUF_SIZE];
    int first = 0;

    strcpy(json, "{\"available\":true");

    add_ll(json, sizeof(json), "configured_processors", sysconf(_SC_NPROCESSORS_CONF), &first);
    add_ll(json, sizeof(json), "online_processors", sysconf(_SC_NPROCESSORS_ONLN), &first);
    add_ll(json, sizeof(json), "clock_ticks_per_second", sysconf(_SC_CLK_TCK), &first);
    add_ll(json, sizeof(json), "page_size_bytes", sysconf(_SC_PAGESIZE), &first);

    add_loadavg(json, sizeof(json), &first);
    add_uptime(json, sizeof(json), &first);
    add_cpu_stat_summary(json, sizeof(json), &first);
    add_context_switches(json, sizeof(json), &first);
    add_thermal_info(json, sizeof(json), &first);
    add_per_core_native(json, sizeof(json), &first);

    strcat(json, "}");
    return (*env)->NewStringUTF(env, json);
}