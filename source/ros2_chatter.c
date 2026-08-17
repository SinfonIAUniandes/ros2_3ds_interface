#include "ros2_chatter.h"

#include "std_msgs_string.h"

void ros2_chatter_init(ros2_chatter *chatter) {
    chatter->topic = DDS_ENTITY_NIL;
    chatter->writer = DDS_ENTITY_NIL;
    chatter->reader = DDS_ENTITY_NIL;
    chatter->last_result = DDS_RETCODE_OK;
    chatter->transmitted = 0;
    chatter->received = 0;
}

bool ros2_chatter_start(ros2_chatter *chatter, dds_entity_t participant) {
    dds_qos_t *writer_qos = dds_create_qos();
    dds_qos_t *reader_qos = dds_create_qos();
    if (writer_qos == NULL || reader_qos == NULL) {
        dds_delete_qos(writer_qos);
        dds_delete_qos(reader_qos);
        ros2_chatter_stop(chatter);
        chatter->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        return false;
    }

    dds_qset_history(writer_qos, DDS_HISTORY_KEEP_LAST, 10);
    dds_qset_reliability(writer_qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_durability(writer_qos, DDS_DURABILITY_VOLATILE);
    dds_qset_history(reader_qos, DDS_HISTORY_KEEP_LAST, 10);
    dds_qset_reliability(reader_qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_durability(reader_qos, DDS_DURABILITY_VOLATILE);
    dds_qset_ignorelocal(reader_qos, DDS_IGNORELOCAL_PARTICIPANT);

    chatter->topic = dds_create_topic(participant, &std_msgs_msg_dds__String__desc, "rt/chatter", NULL, NULL);
    if (chatter->topic < 0) {
        chatter->last_result = chatter->topic;
        goto fail;
    }

    chatter->writer = dds_create_writer(participant, chatter->topic, writer_qos, NULL);
    if (chatter->writer < 0) {
        chatter->last_result = chatter->writer;
        goto fail;
    }

    chatter->reader = dds_create_reader(participant, chatter->topic, reader_qos, NULL);
    if (chatter->reader < 0) {
        chatter->last_result = chatter->reader;
        goto fail;
    }

    dds_delete_qos(writer_qos);
    dds_delete_qos(reader_qos);
    chatter->last_result = DDS_RETCODE_OK;
    return true;

fail:
    dds_return_t result = chatter->last_result;
    dds_delete_qos(writer_qos);
    dds_delete_qos(reader_qos);
    ros2_chatter_stop(chatter);
    chatter->last_result = result;
    return false;
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
    if (chatter->topic > DDS_ENTITY_NIL) {
        dds_return_t result = dds_delete(chatter->topic);
        if (result != DDS_RETCODE_OK) {
            chatter->last_result = result;
        }
    }
    chatter->topic = DDS_ENTITY_NIL;
    chatter->writer = DDS_ENTITY_NIL;
    chatter->reader = DDS_ENTITY_NIL;
}