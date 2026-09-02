#include "ros2_add_two_ints.h"

#include "ros2_common.h"
#include "ros2_types.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int64_t saturated_sum(int64_t a, int64_t b) {
    if (b > 0 && a > INT64_MAX - b) return INT64_MAX;
    if (b < 0 && a < INT64_MIN - b) return INT64_MIN;
    return a + b;
}

static bool make_service_id(ros2_add_two_ints *service, dds_entity_t participant) {
    dds_guid_t participant_guid;
    service->last_result = dds_get_guid(participant, &participant_guid);
    if (service->last_result != DDS_RETCODE_OK) return false;
    uint8_t service_id_bytes[16] = { 0 };
    memcpy(service_id_bytes, participant_guid.v, 12);
    service_id_bytes[15] = 1;
    int length = snprintf(service->service_id, sizeof(service->service_id), "serviceid=%02x",
                          service_id_bytes[0]);
    for (size_t index = 1; index < sizeof(service_id_bytes) && length > 0; index++) {
        length += snprintf(service->service_id + length,
                           sizeof(service->service_id) - (size_t)length,
                           ".%x", service_id_bytes[index]);
    }
    if (length < 0 || (size_t)length + 2 >= sizeof(service->service_id)) return false;
    service->service_id[length++] = ';';
    service->service_id[length] = '\0';
    return true;
}

void ros2_add_two_ints_init(ros2_add_two_ints *service) {
    memset(service, 0, sizeof(*service));
    service->request_topic = DDS_ENTITY_NIL;
    service->response_topic = DDS_ENTITY_NIL;
    service->request_reader = DDS_ENTITY_NIL;
    service->response_writer = DDS_ENTITY_NIL;
    service->last_result = DDS_RETCODE_OK;
}

bool ros2_add_two_ints_start(ros2_add_two_ints *service, dds_entity_t participant) {
    if (!make_service_id(service, participant)) return false;

    ros2_service_interface iface = {
        .name = ROS2_ADD_TWO_INTS_SERVICE,
        .request_name = ROS2_ADD_TWO_INTS_REQUEST_TOPIC,
        .response_name = ROS2_ADD_TWO_INTS_RESPONSE_TOPIC,
        .request_type = &example_interfaces_srv_dds__AddTwoInts_Request__desc,
        .response_type = &example_interfaces_srv_dds__AddTwoInts_Response__desc,
        .request_topic = service->request_topic,
        .response_topic = service->response_topic,
        .request_reader = service->request_reader,
        .response_writer = service->response_writer,
        .last_result = DDS_RETCODE_OK,
        .service_id = service->service_id,
        .running = false
    };

    if (!ros2_service_create_endpoints(&iface, participant, 10, true, DDS_INFINITY, false)) {
        service->last_result = iface.last_result;
        ros2_service_cleanup(&iface);
        service->request_topic = iface.request_topic;
        service->response_topic = iface.response_topic;
        service->request_reader = iface.request_reader;
        service->response_writer = iface.response_writer;
        return false;
    }

    service->request_topic = iface.request_topic;
    service->response_topic = iface.response_topic;
    service->request_reader = iface.request_reader;
    service->response_writer = iface.response_writer;
    service->last_result = iface.last_result;
    service->running = true;
    return true;
}

int32_t ros2_add_two_ints_process(ros2_add_two_ints *service) {
    if (!service->running) {
        service->last_result = DDS_RETCODE_PRECONDITION_NOT_MET;
        return service->last_result;
    }
    int32_t processed = 0;
    for (int attempt = 0; attempt < 8; attempt++) {
        service->take_calls++;
        example_interfaces_srv_dds__AddTwoInts_Request_ request = { 0 };
        void *samples[] = { &request };
        dds_sample_info_t info = { 0 };
        dds_return_t result = dds_take(service->request_reader, samples, &info, 1, 1);
        service->last_take_result = result;
        if (result < 0) {
            service->last_result = result;
            return result;
        }
        if (result == 0) break;
        if (!info.valid_data) {
            service->invalid_samples++;
            continue;
        }
        service->samples_taken++;

        example_interfaces_srv_dds__AddTwoInts_Response_ response = {
            .guid = request.guid,
            .seq = request.seq,
            .sum = saturated_sum(request.a, request.b)
        };
        service->last_result = dds_write(service->response_writer, &response);
        service->last_write_result = service->last_result;
        if (service->last_result != DDS_RETCODE_OK) return service->last_result;
        service->last_request_guid = request.guid;
        service->last_request_seq = request.seq;
        service->last_a = request.a;
        service->last_b = request.b;
        service->last_sum = response.sum;
        service->requests_handled++;
        service->responses_written++;
        processed++;
    }
    service->last_result = DDS_RETCODE_OK;
    return processed;
}

int32_t ros2_add_two_ints_request_matches(ros2_add_two_ints *service) {
    dds_subscription_matched_status_t status = { 0 };
    dds_return_t result = dds_get_subscription_matched_status(service->request_reader, &status);
    if (result < 0) {
        service->last_result = result;
        return result;
    }
    return status.current_count;
}

int32_t ros2_add_two_ints_response_matches(ros2_add_two_ints *service) {
    dds_publication_matched_status_t status = { 0 };
    dds_return_t result = dds_get_publication_matched_status(service->response_writer, &status);
    if (result < 0) {
        service->last_result = result;
        return result;
    }
    return status.current_count;
}

int32_t ros2_add_two_ints_request_incompatible_qos(ros2_add_two_ints *service,
                                                    uint32_t *last_policy_id) {
    dds_requested_incompatible_qos_status_t status = { 0 };
    dds_return_t result = dds_get_requested_incompatible_qos_status(
        service->request_reader, &status);
    if (result < 0) {
        service->last_result = result;
        return result;
    }
    if (last_policy_id != NULL) *last_policy_id = status.last_policy_id;
    return status.total_count;
}

void ros2_add_two_ints_stop(ros2_add_two_ints *service) {
    ros2_service_interface iface = {
        .name = ROS2_ADD_TWO_INTS_SERVICE,
        .request_name = ROS2_ADD_TWO_INTS_REQUEST_TOPIC,
        .response_name = ROS2_ADD_TWO_INTS_RESPONSE_TOPIC,
        .request_type = &example_interfaces_srv_dds__AddTwoInts_Request__desc,
        .response_type = &example_interfaces_srv_dds__AddTwoInts_Response__desc,
        .request_topic = service->request_topic,
        .response_topic = service->response_topic,
        .request_reader = service->request_reader,
        .response_writer = service->response_writer,
        .last_result = service->last_result,
        .service_id = service->service_id,
        .running = service->running
    };

    ros2_service_cleanup(&iface);
    service->request_topic = iface.request_topic;
    service->response_topic = iface.response_topic;
    service->request_reader = iface.request_reader;
    service->response_writer = iface.response_writer;
    service->last_result = iface.last_result;
    service->running = iface.running;
}