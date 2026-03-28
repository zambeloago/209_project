#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Client → Server */
#define MSG_JOIN "JOIN"
#define MSG_MOVE "MOVE"

/* Server → Client */
#define MSG_WAIT    "WAIT"
#define MSG_START   "START"
#define MSG_TURN    "TURN"
#define MSG_INVALID "INVALID"
#define MSG_HIT     "HIT"
#define MSG_MISS    "MISS"
#define MSG_WIN     "WIN"
#define MSG_LOSE    "LOSE"
#define MSG_MESSAGE "MESSAGE"

#endif