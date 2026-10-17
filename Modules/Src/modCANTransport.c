#include "modCANTransport.h"
#include <string.h>

typedef struct { modCANFrame frame; uint32_t queuedAt; } Pending;
static Pending queue[MOD_CAN_TX_QUEUE_SIZE], heartbeat;
static unsigned head, count;
static bool heartbeatPending, active[3], aborting[3], wasBusOff, recovery;
static uint32_t startedAt[3];
static const modCANTransportOps *hardware;
static modCANTransportCounters counters;

void modCANTransportInit(const modCANTransportOps *ops) {
    hardware = ops;
    /* Keep lifetime counters across peripheral recovery. */
    counters.dropped += count + (heartbeatPending ? 1u : 0u);
    head = count = 0;
    heartbeatPending = wasBusOff = recovery = false;
    memset(active, 0, sizeof(active));
    memset(aborting, 0, sizeof(aborting));
}

bool modCANTransportEnqueue(const modCANFrame *frame, uint32_t now, bool priority) {
    if (!hardware || !frame || frame->len > 8 ||
        frame->id > (frame->extended ? 0x1fffffffu : 0x7ffu) || hardware->busOff()) {
        counters.dropped++;
        return false;
    }
    if (priority) {
        /* Status heartbeat has one reserved slot; replace stale snapshots. */
        if (heartbeatPending) counters.dropped++;
        heartbeat.frame = *frame;
        heartbeat.queuedAt = now;
        heartbeatPending = true;
    } else {
        if (count == MOD_CAN_TX_QUEUE_SIZE) { counters.dropped++; return false; }
        unsigned tail = (head + count) % MOD_CAN_TX_QUEUE_SIZE;
        queue[tail].frame = *frame;
        queue[tail].queuedAt = now;
        count++;
    }
    counters.queued++;
    return true;
}

void modCANTransportPoll(uint32_t now) {
    if (!hardware) return;
    bool busOff = hardware->busOff();
    if (busOff && !wasBusOff) {
        counters.busOffEvents++;
        counters.dropped += count + (heartbeatPending ? 1u : 0u);
        count = 0;
        heartbeatPending = false;
    }
    wasBusOff = busOff;
    for (unsigned i = 0; i < 3; i++) {
        modCANMailboxStatus status = hardware->status(i);
        if (active[i]) {
            if (status != MOD_CAN_PENDING) {
                if (!aborting[i] && status == MOD_CAN_SENT) counters.sent++;
                else counters.failed++;
                active[i] = aborting[i] = false;
            } else if (aborting[i]) {
                if ((uint32_t)(now - startedAt[i]) >= MOD_CAN_ABORT_TIMEOUT_MS) recovery = true;
            } else if (busOff || (uint32_t)(now - startedAt[i]) >= MOD_CAN_TX_TIMEOUT_MS) {
                hardware->abort(i);
                aborting[i] = true;
                startedAt[i] = now;
                counters.timedOut++;
            }
        }
        if (busOff || recovery || active[i] || status == MOD_CAN_PENDING) continue;
        Pending next;
        bool have = false;
        while (heartbeatPending || count) {
            if (heartbeatPending) {
                next = heartbeat;
                heartbeatPending = false;
            } else {
                next = queue[head];
                head = (head + 1u) % MOD_CAN_TX_QUEUE_SIZE;
                count--;
            }
            if ((uint32_t)(now - next.queuedAt) < MOD_CAN_QUEUE_MAX_AGE_MS) { have = true; break; }
            counters.dropped++;
        }
        if (have) {
            hardware->submit(i, &next.frame);
            active[i] = true;
            aborting[i] = false;
            startedAt[i] = now;
        }
    }
}

bool modCANTransportNeedsRecovery(void) { return recovery; }
const modCANTransportCounters *modCANTransportGetCounters(void) { return &counters; }
