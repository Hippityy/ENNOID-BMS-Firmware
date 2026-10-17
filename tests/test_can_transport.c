#include "modCANTransport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static modCANMailboxStatus states[3];
static modCANFrame submitted[3];
static bool off;
static unsigned aborts;
static bool bus_off(void) { return off; }
static modCANMailboxStatus status(unsigned i) { return states[i]; }
static void submit(unsigned i, const modCANFrame *f) { submitted[i] = *f; states[i] = MOD_CAN_PENDING; }
static void abort_mailbox(unsigned i) { (void)i; aborts++; }
static const modCANTransportOps ops = {bus_off, status, submit, abort_mailbox};
static void reset(void) {
    off = false; aborts = 0;
    for (unsigned i = 0; i < 3; i++) states[i] = MOD_CAN_EMPTY;
    modCANTransportInit(&ops);
}
static modCANFrame frame(unsigned id) { modCANFrame f = {.id = id, .len = 8, .extended = true}; memset(f.data, id & 255, 8); return f; }
int main(void) {
    reset();
    modCANFrame f = frame(1);
    for (unsigned i = 0; i < MOD_CAN_TX_QUEUE_SIZE; i++) { f = frame(i); assert(modCANTransportEnqueue(&f, 0, false)); }
    f = frame(1000); assert(!modCANTransportEnqueue(&f, 0, false));
    /* A heartbeat still gets through a full telemetry queue, with its own snapshot. */
    assert(modCANTransportEnqueue(&f, 0, true));
    f.data[0] = 99;
    modCANTransportPoll(0);
    assert(submitted[0].id == 1000 && submitted[0].data[0] == (1000 & 255));
    assert(submitted[1].id == 0 && submitted[2].id == 1);
    unsigned next = 2;
    for (unsigned tick = 1; next < MOD_CAN_TX_QUEUE_SIZE; tick++) {
        for (unsigned i = 0; i < 3; i++) states[i] = MOD_CAN_SENT;
        modCANTransportPoll(tick);
        for (unsigned i = 0; i < 3 && next < MOD_CAN_TX_QUEUE_SIZE; i++) assert(submitted[i].id == next++);
    }
    reset(); f = frame(77);
    assert(modCANTransportEnqueue(&f, 100, false)); modCANTransportPoll(100);
    modCANTransportPoll(149); assert(aborts == 0);
    modCANTransportPoll(150); assert(aborts == 1 && !modCANTransportNeedsRecovery());
    modCANTransportPoll(199); assert(aborts == 1);
    modCANTransportPoll(200); assert(modCANTransportNeedsRecovery());
    /* A failed request that acknowledges abort does not force a peripheral reset. */
    reset(); assert(modCANTransportEnqueue(&f, 100, false)); modCANTransportPoll(100);
    modCANTransportPoll(150); states[0] = MOD_CAN_FAILED; modCANTransportPoll(151);
    assert(!modCANTransportNeedsRecovery());
    assert(modCANTransportEnqueue(&f, 152, false)); modCANTransportPoll(152); assert(states[0] == MOD_CAN_PENDING);
    /* Bus-off cannot preserve queued charger/control commands for later replay. */
    assert(modCANTransportEnqueue(&f, 153, false)); off = true; modCANTransportPoll(153);
    assert(!modCANTransportEnqueue(&f, 153, false)); assert(aborts == 2);
    states[0] = MOD_CAN_FAILED; modCANTransportPoll(154); off = false;
    modCANTransportPoll(155); assert(states[1] != MOD_CAN_PENDING);
    assert(modCANTransportEnqueue(&f, 156, true)); modCANTransportPoll(156); assert(states[0] == MOD_CAN_PENDING);
    reset();
    /* Queue expiry and mailbox deadlines work across HAL tick wrap. */
    assert(modCANTransportEnqueue(&f, UINT32_MAX - 100u, false));
    modCANTransportPoll(200); assert(states[0] == MOD_CAN_EMPTY);
    assert(modCANTransportEnqueue(&f, UINT32_MAX - 10u, false));
    modCANTransportPoll(UINT32_MAX - 10u); modCANTransportPoll(40); assert(aborts == 1);
    f.len = 9; assert(!modCANTransportEnqueue(&f, 50, false));
    f.len = 8; f.extended = false; f.id = 0x800; assert(!modCANTransportEnqueue(&f, 50, false));
    puts("CAN transport: queue ordering, saturation, heartbeat priority, ACK loss, abort/recovery, bus-off, stale frames and tick wrap passed");
}
