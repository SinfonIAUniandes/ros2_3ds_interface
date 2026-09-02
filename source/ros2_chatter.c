#include "ros2_chatter.h"

#include "ros2_common.h"
#include "ros2_names.h"
#include "ros2_types.h"

#include <stdio.h>

void ros2_chatter_init(ros2_chatter *chatter) {
    chatter->topic = DDS_ENTITY_NIL;
    chatter->writer = DDS_ENTITY_NIL;
    chatter->reader = DDS_ENTITY_NIL;
    chatter->last_result = DDS_RETCODE_OK;
    chatter->transmitted = 0;
    chatter->received = 0;
}

bool ros2_chatter_start(ros2_chatter *chatter, dds_entity_t participant,
                        const char *ros_namespace) {
    char topic_name[256];
    if (!ros2_dds_name(topic_name, sizeof(topic_name), "rt", ros_namespace, "chatter")) {
        chatter->last_result = DDS_RETCODE_BAD_PARAMETER;
        return false;
    }
    ros2_topic_interface topic = {
        .name = topic_name,
        .type = &std_msgs_msg_dds__String__desc,
        .topic = chatter->topic,
        .writer = chatter->writer,
        .reader = chatter->reader,
        .last_result = DDS_RETCODE_OK,
        .writer_enabled = true,
        .reader_enabled = true
    };

    if (!ros2_topic_create_endpoints(&topic, participant, 10, true, DDS_SECS(10), true)) {
        chatter->last_result = topic.last_result;
        ros2_topic_cleanup(&topic);
        chatter->topic = topic.topic;
        chatter->writer = topic.writer;
        chatter->reader = topic.reader;
        return false;
    }

    chatter->topic = topic.topic;
    chatter->writer = topic.writer;
    chatter->reader = topic.reader;
    chatter->last_result = topic.last_result;
    return true;
}

bool ros2_chatter_publish(ros2_chatter *chatter, const char *data) {
    std_msgs_msg_dds__String_ sample = { .data = (char *)data };
    chatter->last_result = dds_write(chatter->writer, &sample);
    if (chatter->last_result != DDS_RETCODE_OK) {
        return false;
    }

    chatter->transmitted++;
    return true;
}

dds_return_t ros2_chatter_take(ros2_chatter *chatter, ros2_chatter_receive_fn callback, void *context) {
    std_msgs_msg_dds__String_ sample = { .data = NULL };
    void *samples[] = { &sample };
    dds_sample_info_t info;
    dds_return_t result = dds_take(chatter->reader, samples, &info, 1, 1);
    chatter->last_result = result < 0 ? result : DDS_RETCODE_OK;
    if (result <= 0) {
        return result;
    }

    if (info.valid_data) {
        if (callback != NULL) {
            callback(context, sample.data != NULL ? sample.data : "");
        }
        chatter->received++;
    }
    dds_sample_free(&sample, &std_msgs_msg_dds__String__desc, DDS_FREE_CONTENTS);
    return result;
}

dds_entity_t ros2_chatter_writer_entity(const ros2_chatter *chatter) {
    return chatter != NULL ? chatter->writer : DDS_ENTITY_NIL;
}

dds_entity_t ros2_chatter_reader_entity(const ros2_chatter *chatter) {
    return chatter != NULL ? chatter->reader : DDS_ENTITY_NIL;
}

int32_t ros2_chatter_writer_matches(ros2_chatter *chatter) {
    dds_publication_matched_status_t status;
    dds_return_t result = dds_get_publication_matched_status(chatter->writer, &status);
    if (result < 0) {
        chatter->last_result = result;
        return result;
    }
    return status.current_count;
}

int32_t ros2_chatter_reader_matches(ros2_chatter *chatter) {
    dds_subscription_matched_status_t status;
    dds_return_t result = dds_get_subscription_matched_status(chatter->reader, &status);
    if (result < 0) {
        chatter->last_result = result;
        return result;
    }
    return status.current_count;
}

int32_t ros2_chatter_writer_incompatible_qos(ros2_chatter *chatter, uint32_t *last_policy_id) {
    dds_offered_incompatible_qos_status_t status;
    dds_return_t result = dds_get_offered_incompatible_qos_status(chatter->writer, &status);
    if (result < 0) {
        chatter->last_result = result;
        return result;
    }
    if (last_policy_id != NULL) {
        *last_policy_id = status.last_policy_id;
    }
    return (int32_t)status.total_count;
}

int32_t ros2_chatter_reader_incompatible_qos(ros2_chatter *chatter, uint32_t *last_policy_id) {
    dds_requested_incompatible_qos_status_t status;
    dds_return_t result = dds_get_requested_incompatible_qos_status(chatter->reader, &status);
    if (result < 0) {
        chatter->last_result = result;
        return result;
    }
    if (last_policy_id != NULL) {
        *last_policy_id = status.last_policy_id;
    }
    return (int32_t)status.total_count;
}

void ros2_chatter_stop(ros2_chatter *chatter) {
    ros2_topic_interface topic = {
        .name = "rt/chatter",
        .type = &std_msgs_msg_dds__String__desc,
        .topic = chatter->topic,
        .writer = chatter->writer,
        .reader = chatter->reader,
        .last_result = chatter->last_result,
        .writer_enabled = true,
        .reader_enabled = true
    };

    ros2_topic_cleanup(&topic);
    chatter->topic = topic.topic;
    chatter->writer = topic.writer;
    chatter->reader = topic.reader;
    chatter->last_result = topic.last_result;
}
