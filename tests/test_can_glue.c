/* Compile actual glue with real vendor register layouts. The fake registers
 * check emitted MMIO values, not bxCAN arbitration/FIFO/W1C silicon behavior. */
#include "modCAN.h"
#include "modCANTransport.h"
#include <assert.h>
#include <stdio.h>

static CAN_TypeDef fakeCAN;
static RCC_TypeDef fakeRCC;
#undef CAN
#define CAN (&fakeCAN)
#undef RCC
#define RCC (&fakeRCC)
#include "../Modules/Src/modCAN.c"

static uint32_t tick;
static unsigned initCalls, commandCalls;
static bool irqEnabled;
static HAL_StatusTypeDef initResult = HAL_OK, filterResult = HAL_OK;
static modPowerElectronicsPackStateTypedef pack;
static modConfigGeneralConfigStructTypedef config;

uint32_t HAL_GetTick(void) { return tick; }
void HAL_NVIC_DisableIRQ(IRQn_Type irq) { assert(irq == CAN_RX0_IRQn); irqEnabled = false; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq) { assert(irq == CAN_RX0_IRQn); irqEnabled = true; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { assert(irq == CAN_RX0_IRQn); }
HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *handle) {
    initCalls++;
    assert(handle->Instance == CAN);
    assert(handle->Init.ABOM == ENABLE && handle->Init.NART == DISABLE);
    return initResult;
}
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *handle, CAN_FilterConfTypeDef *filter) {
    assert(handle->Instance == CAN && filter->FilterFIFOAssignment == CAN_FIFO0);
    return filterResult;
}
uint8_t modDelayTick1ms(uint32_t *last, uint32_t delay) {
    if ((uint32_t)(tick - *last) < delay) return 0;
    *last = tick;
    return 1;
}
bool modPowerElectronicsHCSafetyCANAndPowerButtonCheck(void) { return true; }
void modPowerElectronicsResetBalanceModeActiveTimeout(void) {}
void modCommandsSetSendFunction(void(*fn)(unsigned char *, unsigned int)) { (void)fn; }
void modCommandsSendPacket(unsigned char *data, unsigned int len) { (void)data; (void)len; }
void modCommandsProcessPacket(unsigned char *data, unsigned int len) {
    assert(data && len);
    commandCalls++;
}

static void inject(uint32_t id, bool extended, uint8_t len, uint8_t byte0, bool remote) {
    CAN->sFIFOMailBox[0].RIR = extended ? (id << 3) | CAN_RI0R_IDE : id << 21;
    if (remote) CAN->sFIFOMailBox[0].RIR |= CAN_RI0R_RTR;
    CAN->sFIFOMailBox[0].RDTR = len;
    CAN->sFIFOMailBox[0].RDLR = byte0;
    CAN->sFIFOMailBox[0].RDHR = 0;
    CAN->RF0R = 1;
    CAN_RX0_IRQHandler();
    assert(CAN->RF0R == CAN_RF0R_RFOM0);
}

static void check_register_writes(void) {
    modCANFrame f = {.id = 0x1234567, .extended = true, .len = 8,
                     .data = {0,1,2,3,4,5,6,7}};
    for (unsigned i = 0; i < 3; i++) {
        CAN->TSR = CAN_TSR_TME0 << i;
        assert(modCANMailbox(i) == MOD_CAN_EMPTY);
        CAN->TSR = 0;
        assert(modCANMailbox(i) == MOD_CAN_PENDING);
        CAN->TSR = (CAN_TSR_RQCP0 | CAN_TSR_TXOK0) << (8u * i);
        assert(modCANMailbox(i) == MOD_CAN_SENT);
        assert(CAN->TSR == CAN_TSR_RQCP0 << (8u * i));
        CAN->TSR = CAN_TSR_RQCP0 << (8u * i);
        assert(modCANMailbox(i) == MOD_CAN_FAILED);
        modCANMailboxAbort(i);
        assert(CAN->TSR == CAN_TSR_ABRQ0 << (8u * i));
        modCANMailboxSubmit(i, &f);
        assert(CAN->sTxMailBox[i].TIR == (f.id << 3 | CAN_TI0R_IDE | CAN_TI0R_TXRQ));
        assert(CAN->sTxMailBox[i].TDTR == 8);
        assert(CAN->sTxMailBox[i].TDLR == 0x03020100 && CAN->sTxMailBox[i].TDHR == 0x07060504);
    }
    f.extended = false; f.id = 0x70A; f.len = 1;
    modCANMailboxSubmit(0, &f);
    assert(CAN->sTxMailBox[0].TIR == ((0x70Au << 21) | CAN_TI0R_TXRQ));
}

static void check_safety_and_ring(void) {
    tick = 100;
    inject(0xA23, true, 1, 3, false);
    modCANSubTaskHandleCommunication();
    assert(pack.safetyOverCANHCSafeNSafe && modCANSafetyCANMessageTimeout == 100);
    /* A repeated fixed arbitration ID remains valid traffic. */
    tick = 150;
    inject(0xA23, true, 1, 3, false);
    modCANSubTaskHandleCommunication();
    assert(modCANSafetyCANMessageTimeout == 150);
    tick = 200;
    inject(0xA23, true, 1, 3, false);
    tick = 6000;
    pack.safetyOverCANHCSafeNSafe = false;
    modCANSubTaskHandleCommunication();
    assert(!pack.safetyOverCANHCSafeNSafe && modCANSafetyCANMessageTimeout == 150);
    config.chargerCANProtocol = 2;
    tick = 200;
    inject(0x70A, false, 1, 5, false);
    tick = 6000;
    modCANSubTaskHandleCommunication();
    assert(modCANLastChargerHeartBeatTick == 0);
    config.chargerCANProtocol = 0;
    tick = 150;
    inject(0xA23, true, 1, 3, false);
    modCANSubTaskHandleCommunication();
    uint32_t dropped = modCANRxDropped;
    inject(0xA23, true, 9, 3, false);
    inject(0xA23, true, 1, 3, true);
    assert(modCANRxDropped == dropped + 2);
    modCANSubTaskHandleCommunication();
    assert(modCANSafetyCANMessageTimeout == 150);
    assert(modCANInit(&pack, &config));
    assert(modCANSafetyCANMessageTimeout == 150); /* recovery cannot refresh safety */
    tick = 5149; modCANTask(); assert(pack.safetyOverCANHCSafeNSafe);
    tick = 5150; modCANTask(); assert(!pack.safetyOverCANHCSafeNSafe);

    tick = UINT32_MAX - 1000;
    inject(0xA23, true, 1, 3, false);
    modCANTask();
    tick = 500; modCANTask(); assert(pack.safetyOverCANHCSafeNSafe);
    tick = 4000; modCANTask(); assert(!pack.safetyOverCANHCSafeNSafe);

    dropped = modCANRxDropped;
    for (unsigned n = 0; n < RX_CAN_FRAMES_SIZE; n++) inject(0x1234, true, 0, 0, false);
    assert(modCANRxDropped == dropped + 1);
    uint8_t before = modCANRxFrameRead;
    modCANSubTaskHandleCommunication();
    assert(modCANRxFrameRead == (before + 32u) % RX_CAN_FRAMES_SIZE);
    for (unsigned n = 0; n < 4; n++) modCANSubTaskHandleCommunication();
    assert(modCANRxFrameRead == modCANRxFrameWrite);
}

static void check_init_failures(void) {
    tick = 7000;
    initResult = HAL_ERROR;
    assert(!modCANInit(&pack, &config) && !irqEnabled && !CAN->IER);
    unsigned calls = initCalls;
    tick = 7249; modCANTask(); assert(initCalls == calls);
    initResult = HAL_OK;
    tick = 7250; modCANTask(); assert(initCalls == calls + 1 && irqEnabled && modCANReady);
    filterResult = HAL_ERROR;
    assert(!modCANInit(&pack, &config) && !irqEnabled && !CAN->IER);
    filterResult = HAL_OK;
    assert(modCANInit(&pack, &config));
}

int main(void) {
    config.CANID = 10;
    config.canBusSpeed = canSpeedBaud500k;
    tick = 1;
    assert(modCANInit(&pack, &config));
    assert(irqEnabled && CAN->IER == (CAN_IER_FMPIE0 | CAN_IER_FOVIE0));
    check_register_writes();
    modCANTransmitExtID(123, NULL, 0);
    modCANTransmitStandardID(123, NULL, 0);
    check_safety_and_ring();
    check_init_failures();
    assert(!commandCalls);
    puts("CAN glue: production MMIO encodings, RX validation/ring budget, safety deadlines/tick wrap and bounded init retry passed");
    return 0;
}
