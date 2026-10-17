#ifndef MOD_CAN_TRANSPORT_H
#define MOD_CAN_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>

#define MOD_CAN_TX_QUEUE_SIZE 128
#define MOD_CAN_TX_TIMEOUT_MS 50u
#define MOD_CAN_ABORT_TIMEOUT_MS 50u
#define MOD_CAN_QUEUE_MAX_AGE_MS 250u

typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
    bool extended;
} modCANFrame;
typedef enum { MOD_CAN_PENDING, MOD_CAN_EMPTY, MOD_CAN_SENT, MOD_CAN_FAILED } modCANMailboxStatus;
typedef struct {
    bool (*busOff)(void);
    modCANMailboxStatus (*status)(unsigned mailbox);
    void (*submit)(unsigned mailbox, const modCANFrame *frame);
    void (*abort)(unsigned mailbox);
} modCANTransportOps;
typedef struct {
    uint32_t queued, sent, failed, dropped, timedOut, busOffEvents;
} modCANTransportCounters;

void modCANTransportInit(const modCANTransportOps *ops);
bool modCANTransportEnqueue(const modCANFrame *frame, uint32_t now, bool priority);
void modCANTransportPoll(uint32_t now);
bool modCANTransportNeedsRecovery(void);
const modCANTransportCounters *modCANTransportGetCounters(void);
#endif
