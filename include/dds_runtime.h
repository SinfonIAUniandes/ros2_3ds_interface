#ifndef ROS2_3DS_DDS_RUNTIME_H
#define ROS2_3DS_DDS_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*dds_runtime_log_fn)(void *context, int level, const char *message);

typedef struct {
    int32_t participant;
    int32_t last_result;
    bool running;
} dds_runtime;

void dds_runtime_init(dds_runtime *runtime);
void dds_runtime_set_log_sink(dds_runtime_log_fn callback, void *context);
bool dds_runtime_start(dds_runtime *runtime, uint32_t domain_id);
void dds_runtime_stop(dds_runtime *runtime);
const char *dds_runtime_status(const dds_runtime *runtime);
const char *dds_runtime_error_text(const dds_runtime *runtime);

#endif
