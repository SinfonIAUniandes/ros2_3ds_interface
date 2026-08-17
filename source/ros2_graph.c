#include "ros2_graph.h"

#include "ros_graph.h"

#include <stdio.h>
#include <string.h>

void ros2_graph_init(ros2_graph *graph) {
    graph->topic = DDS_ENTITY_NIL;
    graph->writer = DDS_ENTITY_NIL;
    graph->last_result = DDS_RETCODE_OK;
    graph->published = 0;
}

bool ros2_graph_start(ros2_graph *graph, dds_entity_t participant) {
    dds_qos_t *qos = dds_create_qos();
    if (qos == NULL) {
        graph->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        return false;
    }

    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
    graph->topic = dds_create_topic(participant,
                                    &rmw_dds_common_msg_dds__ParticipantEntitiesInfo__desc,
                                    ROS2_GRAPH_TOPIC, NULL, NULL);
    if (graph->topic < 0) {
        graph->last_result = graph->topic;
        goto fail;
    }

    graph->writer = dds_create_writer(participant, graph->topic, qos, NULL);
    if (graph->writer < 0) {
        graph->last_result = graph->writer;
        goto fail;
    }

    dds_delete_qos(qos);
    graph->last_result = DDS_RETCODE_OK;
    return true;

fail:
    dds_delete_qos(qos);
    dds_return_t result = graph->last_result;
    ros2_graph_stop(graph);
    graph->last_result = result;
    return false;
}

bool ros2_graph_publish(ros2_graph *graph, dds_entity_t participant,
                        dds_entity_t chatter_writer, dds_entity_t chatter_reader) {
    dds_guid_t participant_guid;
    dds_guid_t writer_guid;
    dds_guid_t reader_guid;
    graph->last_result = dds_get_guid(participant, &participant_guid);
    if (graph->last_result != DDS_RETCODE_OK) {
        return false;
    }
    graph->last_result = dds_get_guid(chatter_writer, &writer_guid);
    if (graph->last_result != DDS_RETCODE_OK) {
        return false;
    }
    graph->last_result = dds_get_guid(chatter_reader, &reader_guid);
    if (graph->last_result != DDS_RETCODE_OK) {
        return false;
    }

    rmw_dds_common_msg_dds__Gid_ writer_gid = { { 0 } };
    rmw_dds_common_msg_dds__Gid_ reader_gid = { { 0 } };
    rmw_dds_common_msg_dds__NodeEntitiesInfo_ node = { 0 };
    rmw_dds_common_msg_dds__ParticipantEntitiesInfo_ sample = { 0 };
    memcpy(sample.gid.data, participant_guid.v, sizeof(sample.gid.data));
    memcpy(writer_gid.data, writer_guid.v, sizeof(writer_gid.data));
    memcpy(reader_gid.data, reader_guid.v, sizeof(reader_gid.data));
    snprintf(node.node_namespace, sizeof(node.node_namespace), "%s", ROS2_GRAPH_NODE_NAMESPACE);
    snprintf(node.node_name, sizeof(node.node_name), "%s", ROS2_GRAPH_NODE_NAME);
    node.reader_gid_seq._maximum = 1;
    node.reader_gid_seq._length = 1;
    node.reader_gid_seq._buffer = &reader_gid;
    node.reader_gid_seq._release = false;
    node.writer_gid_seq._maximum = 1;
    node.writer_gid_seq._length = 1;
    node.writer_gid_seq._buffer = &writer_gid;
    node.writer_gid_seq._release = false;
    sample.node_entities_info_seq._maximum = 1;
    sample.node_entities_info_seq._length = 1;
    sample.node_entities_info_seq._buffer = &node;
    sample.node_entities_info_seq._release = false;

    graph->last_result = dds_write(graph->writer, &sample);
    if (graph->last_result != DDS_RETCODE_OK) {
        return false;
    }
    graph->published++;
    return true;
}

int32_t ros2_graph_writer_matches(ros2_graph *graph) {
    dds_publication_matched_status_t status = { 0 };
    graph->last_result = dds_get_publication_matched_status(graph->writer, &status);
    if (graph->last_result != DDS_RETCODE_OK) {
        return graph->last_result;
    }
    return status.current_count;
}

void ros2_graph_stop(ros2_graph *graph) {
    if (graph->topic > DDS_ENTITY_NIL) {
        dds_return_t result = dds_delete(graph->topic);
        if (result != DDS_RETCODE_OK) {
            graph->last_result = result;
        }
    }
    graph->topic = DDS_ENTITY_NIL;
    graph->writer = DDS_ENTITY_NIL;
}