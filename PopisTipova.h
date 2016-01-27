#pragma once

/* TIPOVI ADRESA*/
#define ADR_IPv4        0x01
#define ADR_IPv6        0x02
#define ADR_HOSTNAME    0x03

/*TIPOVI PORUKA*/
#define MSG_PING                  0x01
#define MSG_PONG                  0x02
#define MSG_PONG_REG_REQ          0x03
#define MSG_STREAM_ADVERTISEMENT  0x04
#define MSG_STREAM_REGISTERED     0x05
#define MSG_IDENTIFIER_NOT_USABLE 0x06
#define MSG_FIND_STREAM_SOURCE    0x07
#define MSG_STREAM_SOURCE_DATA    0x08
#define MSG_STREAM_REMOVE         0x09
#define MSG_MULTIMEDIA            0x0a
#define MSG_REQUEST_STREAMING     0x0b
#define MSG_FORWARD_PLAYER_READY  0x0c
#define MSG_PLAYER_READY          0x0d
#define MSG_SOURCE_READY          0x0e
#define MSG_REQ_RELAY_LIST        0x0f
#define MSG_RELAY_LIST            0x10
#define MSG_SHUTTING_DOWN         0x11
#define MSG_PLEASE_FORWARD        0x12
#define MSG_REGISTER_FORWARDING   0x13