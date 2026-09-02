#include "ros2_common.h"

#include <stdio.h>
#include <string.h>

bool ros2_create_qos(dds_qos_t **qos, int history_depth, bool reliable, int64_t lease_ns,
                     bool ignore_local, const char *service_id,
                     bool disable_writer_data_lifecycle) {
    if (qos == NULL) {
        return false;
    }

    *qos = dds_create_qos();
    if (*qos == NULL) {
        return false;
    }

    dds_qset_history(*qos, DDS_HISTORY_KEEP_LAST, history_depth > 0 ? history_depth : 1);
    dds_qset_reliability(*qos, reliable ? DDS_RELIABILITY_RELIABLE : DDS_RELIABILITY_BEST_EFFORT,
                         lease_ns > 0 ? DDS_MSECS(lease_ns / 1000000LL) : DDS_INFINITY);
    dds_qset_durability(*qos, DDS_DURABILITY_VOLATILE);
    if (disable_writer_data_lifecycle) {
        dds_qset_writer_data_lifecycle(*qos, false);
    }
    if (ignore_local) {
        dds_qset_ignorelocal(*qos, DDS_IGNORELOCAL_PARTICIPANT);
    }
    if (service_id != NULL && service_id[0] != '\0') {
        char user_data[96];
        int length = snprintf(user_data, sizeof(user_data), "%sros2_3ds=1;", service_id);
        if (length < 0 || (size_t)length >= sizeof(user_data)) {
            dds_delete_qos(*qos);
            *qos = NULL;
            return false;
        }
        dds_qset_userdata(*qos, user_data, (size_t)length);
    } else {
        static const char user_data[] = "ros2_3ds=1;";
        dds_qset_userdata(*qos, user_data, sizeof(user_data) - 1u);
    }
    return true;
}

void ros2_destroy_qos(dds_qos_t *qos) {
    if (qos != NULL) {
        dds_delete_qos(qos);
    }
}

bool ros2_topic_create_endpoints(ros2_topic_interface *interface, dds_entity_t participant,
                                int history_depth, bool reliable, int64_t lease_ns,
                                bool ignore_local) {
    if (interface == NULL || participant <= DDS_ENTITY_NIL || interface->type == NULL) {
        return false;
    }

    dds_qos_t *writer_qos = NULL;
    dds_qos_t *reader_qos = NULL;
    if (!ros2_create_qos(&writer_qos, history_depth, reliable, lease_ns, ignore_local, NULL, false) ||
        !ros2_create_qos(&reader_qos, history_depth, reliable, lease_ns, ignore_local, NULL, false)) {
        ros2_destroy_qos(writer_qos);
        ros2_destroy_qos(reader_qos);
        return false;
    }

    interface->topic = dds_create_topic(participant, interface->type, interface->name, NULL, NULL);
    if (interface->topic < 0) {
        interface->last_result = interface->topic;
        ros2_destroy_qos(writer_qos);
        ros2_destroy_qos(reader_qos);
        return false;
    }

    if (interface->writer_enabled) {
        interface->writer = dds_create_writer(participant, interface->topic, writer_qos, NULL);
        if (interface->writer < 0) {
            interface->last_result = interface->writer;
            ros2_destroy_qos(writer_qos);
            ros2_destroy_qos(reader_qos);
            ros2_topic_cleanup(interface);
            return false;
        }
    }

    if (interface->reader_enabled) {
        interface->reader = dds_create_reader(participant, interface->topic, reader_qos, NULL);
        if (interface->reader < 0) {
            interface->last_result = interface->reader;
            ros2_destroy_qos(writer_qos);
            ros2_destroy_qos(reader_qos);
            ros2_topic_cleanup(interface);
            return false;
        }
    }

    ros2_destroy_qos(writer_qos);
    ros2_destroy_qos(reader_qos);
    interface->last_result = DDS_RETCODE_OK;
    return true;
}

void ros2_topic_cleanup(ros2_topic_interface *interface) {
    if (interface == NULL) {
        return;
    }

    if (interface->writer > DDS_ENTITY_NIL) {
        if (dds_delete(interface->writer) != DDS_RETCODE_OK) {
            interface->last_result = DDS_RETCODE_ERROR;
        }
        interface->writer = DDS_ENTITY_NIL;
    }
    if (interface->reader > DDS_ENTITY_NIL) {
        if (dds_delete(interface->reader) != DDS_RETCODE_OK) {
            interface->last_result = DDS_RETCODE_ERROR;
        }
        interface->reader = DDS_ENTITY_NIL;
    }
    if (interface->topic > DDS_ENTITY_NIL) {
        if (dds_delete(interface->topic) != DDS_RETCODE_OK) {
            interface->last_result = DDS_RETCODE_ERROR;
        }
        interface->topic = DDS_ENTITY_NIL;
    }
}

bool ros2_service_create_endpoints(ros2_service_interface *interface, dds_entity_t participant,
                                  int history_depth, bool reliable, int64_t lease_ns,
                                  bool ignore_local) {
    if (interface == NULL || participant <= DDS_ENTITY_NIL ||
        interface->request_type == NULL || interface->response_type == NULL) {
        return false;
    }

    dds_qos_t *request_qos = NULL;
    dds_qos_t *response_qos = NULL;
    if (!ros2_create_qos(&request_qos, history_depth, reliable, lease_ns, ignore_local,
                         interface->service_id, true) ||
        !ros2_create_qos(&response_qos, history_depth, reliable, lease_ns, ignore_local,
                         interface->service_id, true)) {
        ros2_destroy_qos(request_qos);
        ros2_destroy_qos(response_qos);
        return false;
    }

    interface->request_topic = dds_create_topic(participant, interface->request_type,
                                               interface->request_name, NULL, NULL);
    if (interface->request_topic < 0) {
        interface->last_result = interface->request_topic;
        ros2_destroy_qos(request_qos);
        ros2_destroy_qos(response_qos);
        return false;
    }

    interface->response_topic = dds_create_topic(participant, interface->response_type,
                                                interface->response_name, NULL, NULL);
    if (interface->response_topic < 0) {
        interface->last_result = interface->response_topic;
        ros2_destroy_qos(request_qos);
        ros2_destroy_qos(response_qos);
        ros2_service_cleanup(interface);
        return false;
    }

    interface->request_reader = dds_create_reader(participant, interface->request_topic,
                                                 request_qos, NULL);
    if (interface->request_reader < 0) {
        interface->last_result = interface->request_reader;
        ros2_destroy_qos(request_qos);
        ros2_destroy_qos(response_qos);
        ros2_service_cleanup(interface);
        return false;
    }

    interface->response_writer = dds_create_writer(participant, interface->response_topic,
                                                 response_qos, NULL);
    if (interface->response_writer < 0) {
        interface->last_result = interface->response_writer;
        ros2_destroy_qos(request_qos);
        ros2_destroy_qos(response_qos);
        ros2_service_cleanup(interface);
        return false;
    }

    ros2_destroy_qos(request_qos);
    ros2_destroy_qos(response_qos);
    interface->last_result = DDS_RETCODE_OK;
    interface->running = true;
    return true;
}

void ros2_service_cleanup(ros2_service_interface *interface) {
    if (interface == NULL) {
        return;
    }

    if (interface->request_reader > DDS_ENTITY_NIL) {
        if (dds_delete(interface->request_reader) != DDS_RETCODE_OK) {
            interface->last_result = DDS_RETCODE_ERROR;
        }
        interface->request_reader = DDS_ENTITY_NIL;
    }
    if (interface->response_writer > DDS_ENTITY_NIL) {
        if (dds_delete(interface->response_writer) != DDS_RETCODE_OK) {
            interface->last_result = DDS_RETCODE_ERROR;
        }
        interface->response_writer = DDS_ENTITY_NIL;
    }
    if (interface->request_topic > DDS_ENTITY_NIL) {
        if (dds_delete(interface->request_topic) != DDS_RETCODE_OK) {
            interface->last_result = DDS_RETCODE_ERROR;
        }
        interface->request_topic = DDS_ENTITY_NIL;
    }
    if (interface->response_topic > DDS_ENTITY_NIL) {
        if (dds_delete(interface->response_topic) != DDS_RETCODE_OK) {
            interface->last_result = DDS_RETCODE_ERROR;
        }
        interface->response_topic = DDS_ENTITY_NIL;
    }
    interface->running = false;
}
