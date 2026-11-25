#include "protocol/packet.h"

uint8_t stream_of(uint8_t type) {
    switch (type) {
        case MSG_CHUNK_STORE:
        case MSG_CHUNK_REQUEST:
        case MSG_CHUNK_DATA:
        case MSG_CHUNK_ERR:
            return STREAM_FILE;
        default:
            return STREAM_CONTROL;
    }
}

bool is_known_type(uint8_t type) {
    return type >= MSG_DISCOVER && type <= MSG_HAVE_MAP;
}

const char* msg_type_name(uint8_t type) {
    switch (type) {
        case MSG_DISCOVER: return "DISCOVER";
        case MSG_JOIN: return "JOIN";
        case MSG_WELCOME: return "WELCOME";
        case MSG_HEARTBEAT: return "HEARTBEAT";
        case MSG_LEAVE: return "LEAVE";
        case MSG_CHAT: return "CHAT";
        case MSG_ACK: return "ACK";
        case MSG_FILE_ANNOUNCE: return "FILE_ANNOUNCE";
        case MSG_CHUNK_STORE: return "CHUNK_STORE";
        case MSG_HAVE: return "HAVE";
        case MSG_CHUNK_REQUEST: return "CHUNK_REQUEST";
        case MSG_CHUNK_DATA: return "CHUNK_DATA";
        case MSG_CHUNK_ERR: return "CHUNK_ERR";
        case MSG_HAVE_MAP: return "HAVE_MAP";
        default: return "UNKNOWN";
    }
}
