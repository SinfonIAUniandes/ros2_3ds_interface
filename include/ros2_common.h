#ifndef ROS2_3DS_ROS2_COMMON_H
#define ROS2_3DS_ROS2_COMMON_H

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>

typedef struct {
    const char *name;
    const dds_topic_descriptor_t *type;
    dds_entity_t topic;
    dds_entity_t writer;
    dds_entity_t reader;
    dds_return_t last_result;
    bool writer_enabled;
    bool reader_enabled;
} ros2_topic_interface;

typedef struct {
    const char *name;
    const char *request_name;
    const char *response_name;
    const dds_topic_descriptor_t *request_type;
    const dds_topic_descriptor_t *response_type;
    dds_entity_t request_topic;
    dds_entity_t response_topic;
    dds_entity_t request_reader;
    dds_entity_t response_writer;
    dds_return_t last_result;
    const char *service_id;
    bool running;
} ros2_service_interface;

bool ros2_create_qos(dds_qos_t **qos, int history_depth, bool reliable, int64_t lease_ns,
                     bool ignore_local, const char *service_id,
                     bool disable_writer_data_lifecycle);
void ros2_destroy_qos(dds_qos_t *qos);
bool ros2_topic_create_endpoints(ros2_topic_interface *interface, dds_entity_t participant,
                                int history_depth, bool reliable, int64_t lease_ns,
                                bool ignore_local);
void ros2_topic_cleanup(ros2_topic_interface *interface);
bool ros2_service_create_endpoints(ros2_service_interface *interface, dds_entity_t participant,
                                  int history_depth, bool reliable, int64_t lease_ns,
                                  bool ignore_local);
void ros2_service_cleanup(ros2_service_interface *interface);

#endif
