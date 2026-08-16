#include "dds_runtime.h"

#include <dds/dds.h>

#include <string.h>

static dds_runtime_log_fn g_log_callback;
static void *g_log_context;

static void dds_log_sink(void *context, const dds_log_data_t *data) {
    (void)context;
    if (g_log_callback == NULL || data == NULL || data->message == NULL) {
        return;
    }

    char message[160];
    size_t length = data->size < sizeof(message) - 1 ? data->size : sizeof(message) - 1;
    memcpy(message, data->message, length);
    while (length > 0 && (message[length - 1] == '\n' || message[length - 1] == '\r')) {
        length--;
    }
    message[length] = '\0';
    int level = (data->priority & DDS_LC_ERROR) || (data->priority & DDS_LC_FATAL)
        ? 3 : (data->priority & DDS_LC_WARNING) ? 2 : 4;
    g_log_callback(g_log_context, level, message);
}

void dds_runtime_init(dds_runtime *runtime) {
    runtime->participant = DDS_ENTITY_NIL;
    runtime->last_result = DDS_RETCODE_OK;
    runtime->running = false;
}

void dds_runtime_set_log_sink(dds_runtime_log_fn callback, void *context) {
    g_log_callback = callback;
    g_log_context = context;
    dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_INFO);
    dds_set_log_sink(dds_log_sink, NULL);
    dds_set_trace_sink(NULL, NULL);
}

bool dds_runtime_start(dds_runtime *runtime, uint32_t domain_id) {
    if (runtime->running) {
        return true;
    }

    dds_entity_t participant = dds_create_participant(domain_id, NULL, NULL);
    if (participant < 0) {
        runtime->last_result = participant;
        return false;
    }

    runtime->participant = participant;
    runtime->last_result = DDS_RETCODE_OK;
    runtime->running = true;
    return true;
}

void dds_runtime_stop(dds_runtime *runtime) {
    if (!runtime->running) {
        return;
    }

    runtime->last_result = dds_delete(runtime->participant);
    runtime->participant = DDS_ENTITY_NIL;
    runtime->running = false;
    dds_set_log_sink(NULL, NULL);
    dds_set_trace_sink(NULL, NULL);
    g_log_callback = NULL;
    g_log_context = NULL;
}

const char *dds_runtime_status(const dds_runtime *runtime) {
    return runtime->running ? "RUNNING" : "STOPPED";
}

const char *dds_runtime_error_text(const dds_runtime *runtime) {
    return dds_strretcode(runtime->last_result);
}
