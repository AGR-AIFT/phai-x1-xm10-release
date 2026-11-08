/**
 * @file    main.c
 * @brief   NUCLEO-G474RE — XM10 예제 Ex.45(CAN-FD My Sensor)의 상대 장치(참조 구현)
 * @details 하는 일
 *            - 1 ms 마다 주기 데이터(카운터 + ADC 2채널, 16 바이트)를 XM10 으로 보냅니다
 *            - 1 초마다 살아있음을 보냅니다(켜질 때 재시작 알림 한 번)
 *            - XM10 이 보낸 LED 명령으로 LD2 를 켜고 끕니다
 *          XM10 과 주고받는 규칙은 canfd_sensor.c 에 있고, 이 파일은 G474 의 HAL 로 그 규칙이 쓰는 함수만 채웁니다.
 *
 *  [필요한 것]  NUCLEO-G474RE · CAN-FD 트랜시버 보드(5 Mbit/s 지원, 3.3 V 로직)
 *  [배선]       PA11(FDCAN1_RX) → 트랜시버 RXD · PA12(FDCAN1_TX) → 트랜시버 TXD · 3.3 V · GND
 *               트랜시버 CAN_H · CAN_L · GND → XM10 CAN-FD #2 포트. 버스 양 끝에 120 Ω 종단(XM10 쪽에는 보드에 있습니다)
 *  [ADC]        PA0 → ADC 0 · PA1 → ADC 1 (0 ~ 3.3 V, 12 비트)
 *
 * @note  클럭: 보드의 24 MHz 크리스털(HSE) → PLL 320 MHz → 시스템 160 MHz · FDCAN 80 MHz.
 *        CAN-FD 5 Mbit/s 는 클럭이 정확해야 해서 내부 RC(HSI) 대신 크리스털을 씁니다.
 */

#include "main.h"
#include "canfd_sensor.h"

FDCAN_HandleTypeDef hfdcan1;
ADC_HandleTypeDef   hadc1;

static uint32_t s_busoff_count;     /* 버스 오류(bus-off)에서 다시 시작한 수 — 디버거로 봅니다 */
static uint32_t s_busoff_check_ms;

#define BUSOFF_CHECK_MS   (10U)
#define ADC_TIMEOUT_MS    (1U)

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_FDCAN1_Init(void);
static void _ReceiveAll(uint32_t now_ms);
static void _RecoverBusOff(uint32_t now_ms);
static uint32_t _LenToDlc(uint8_t len);
static uint8_t  _DlcToLen(uint32_t dlc);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_FDCAN1_Init();

    Sensor_Init(HAL_GetTick());

    while (1) {
        const uint32_t now = HAL_GetTick();
        _ReceiveAll(now);
        Sensor_Poll(now);
        _RecoverBusOff(now);
    }
}

/* ---------------------------------------------------------------- canfd_sensor.h 가 쓰는 함수 */

bool Sensor_PortSend(uint16_t id, const uint8_t* data, uint8_t len)
{
    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0U) {
        return false;                                     /* 보내기 큐가 가득 — 기다리지 않습니다 */
    }
    FDCAN_TxHeaderTypeDef tx = {0};
    tx.Identifier          = id;
    tx.IdType              = FDCAN_STANDARD_ID;
    tx.TxFrameType         = FDCAN_DATA_FRAME;
    tx.DataLength          = _LenToDlc(len);              /* len 은 CAN-FD 길이(0~8 · 12 · 16 · 20 · 24 · 32 · 48 · 64)로 줍니다 */
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.BitRateSwitch       = FDCAN_BRS_ON;
    tx.FDFormat            = FDCAN_FD_CAN;
    tx.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx.MessageMarker       = 0U;
    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx, data) == HAL_OK;
}

void Sensor_PortReadAdc(uint16_t* adc0, uint16_t* adc1)
{
    /* 두 채널을 차례로 한 번 변환합니다(합쳐 수 µs) */
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return;
    }
    if (HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) == HAL_OK) {
        *adc0 = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    if (HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) == HAL_OK) {
        *adc1 = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    (void)HAL_ADC_Stop(&hadc1);
}

void Sensor_PortSetLed(bool on)
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---------------------------------------------------------------- 받기 · 버스 오류 */

/** @brief 받은 메시지를 모두 꺼내 규칙 쪽으로 넘깁니다(필터가 명령 · 동기 신호만 들여보냅니다) */
static void _ReceiveAll(uint32_t now_ms)
{
    FDCAN_RxHeaderTypeDef rx;
    uint8_t data[64];

    while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0U) {
        if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rx, data) != HAL_OK) {
            break;
        }
        if ((rx.IdType == FDCAN_STANDARD_ID) && (rx.RxFrameType == FDCAN_DATA_FRAME)) {
            Sensor_OnFrame((uint16_t)rx.Identifier, data, _DlcToLen(rx.DataLength), now_ms);
        }
    }
}

/**
 * @brief 버스 오류(bus-off)에서 다시 시작합니다.
 * @details 배선이 빠졌거나 속도가 틀리면 송신 오류가 쌓여 FDCAN 이 버스에서 빠집니다. 다시 시작하면 하드웨어가 버스가
 *          조용해지기를 기다린 뒤 돌아옵니다.
 */
static void _RecoverBusOff(uint32_t now_ms)
{
    if ((now_ms - s_busoff_check_ms) < BUSOFF_CHECK_MS) {
        return;
    }
    s_busoff_check_ms = now_ms;

    FDCAN_ProtocolStatusTypeDef ps;
    if ((HAL_FDCAN_GetProtocolStatus(&hfdcan1, &ps) == HAL_OK) && (ps.BusOff != 0U)) {
        s_busoff_count++;
        (void)HAL_FDCAN_Stop(&hfdcan1);
        (void)HAL_FDCAN_Start(&hfdcan1);
    }
}

/** @brief 바이트 수 → 길이 코드. CAN-FD 길이가 아니면 그보다 큰 가장 가까운 길이로 올립니다(data 는 그만큼 있어야 합니다) */
static uint32_t _LenToDlc(uint8_t len)
{
    static const uint32_t k_dlc[9] = {
        FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2, FDCAN_DLC_BYTES_3, FDCAN_DLC_BYTES_4,
        FDCAN_DLC_BYTES_5, FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7, FDCAN_DLC_BYTES_8,
    };
    if (len <= 8U)  { return k_dlc[len]; }
    if (len <= 12U) { return FDCAN_DLC_BYTES_12; }
    if (len <= 16U) { return FDCAN_DLC_BYTES_16; }
    if (len <= 20U) { return FDCAN_DLC_BYTES_20; }
    if (len <= 24U) { return FDCAN_DLC_BYTES_24; }
    if (len <= 32U) { return FDCAN_DLC_BYTES_32; }
    if (len <= 48U) { return FDCAN_DLC_BYTES_48; }
    return FDCAN_DLC_BYTES_64;
}

/** @brief 길이 코드 → 바이트 수. 이 HAL 판은 길이 코드(0 ~ 15)를 그대로 줍니다 */
static uint8_t _DlcToLen(uint32_t dlc)
{
    static const uint8_t k_len[16] = { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U };
    return k_len[dlc & 0x0FU];
}

/* ---------------------------------------------------------------- 초기화 */

/**
 * @brief 시스템 160 MHz · FDCAN 80 MHz — HSE 24 MHz ÷ 6 × 80 = 320 MHz, R ÷ 2 = 160 MHz, Q ÷ 4 = 80 MHz
 * @note  150 MHz 를 넘어 전압 범위 1 부스트를 씁니다. 플래시 대기 상태는 ST 템플릿(NUCLEO-G474RE)과 같은 값입니다.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
    __HAL_RCC_PWR_CLK_DISABLE();

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = RCC_PLLM_DIV6;
    osc.PLL.PLLN       = 80U;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = RCC_PLLQ_DIV4;
    osc.PLL.PLLR       = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();                                   /* 크리스털이 돌지 않으면 여기서 멈춥니다(LD2 빠른 깜빡임) */
    }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_8) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = LD2_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &gpio);
}

/** @brief ADC1 — 채널 1(PA0) · 2(PA1) 를 소프트웨어 시작으로 차례로 한 번씩 변환합니다 */
static void MX_ADC1_Init(void)
{
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;   /* 160 MHz ÷ 4 = 40 MHz */
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.GainCompensation      = 0U;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 2U;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode      = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    ADC_ChannelConfTypeDef ch = {0};
    ch.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    ch.SingleDiff   = ADC_SINGLE_ENDED;
    ch.OffsetNumber = ADC_OFFSET_NONE;
    ch.Offset       = 0U;
    ch.Channel      = ADC_CHANNEL_1;
    ch.Rank         = ADC_REGULAR_RANK_1;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) {
        Error_Handler();
    }
    ch.Channel = ADC_CHANNEL_2;
    ch.Rank    = ADC_REGULAR_RANK_2;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief FDCAN1 — XM10 과 같은 비트 타이밍(FDCAN 클럭 80 MHz 기준)
 * @details 중재 구간 80 MHz ÷ 10 ÷ (1 + 5 + 2) = 1 Mbit/s · 샘플 포인트 (1 + 5) ÷ 8 = 75 %
 *          데이터 구간 80 MHz ÷ 1 ÷ (1 + 11 + 4) = 5 Mbit/s · 샘플 포인트 (1 + 11) ÷ 16 = 75 %
 *          5 Mbit/s 는 송신 지연 보상(TDC)이 필요합니다 — 오프셋 = 데이터 분주 × 데이터 seg1 = 11
 */
static void MX_FDCAN1_Init(void)
{
    hfdcan1.Instance                  = FDCAN1;
    hfdcan1.Init.ClockDivider         = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_FD_BRS;
    hfdcan1.Init.Mode                 = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission   = ENABLE;
    hfdcan1.Init.TransmitPause        = ENABLE;
    hfdcan1.Init.ProtocolException    = DISABLE;
    hfdcan1.Init.NominalPrescaler     = 10U;
    hfdcan1.Init.NominalSyncJumpWidth = 2U;
    hfdcan1.Init.NominalTimeSeg1      = 5U;
    hfdcan1.Init.NominalTimeSeg2      = 2U;
    hfdcan1.Init.DataPrescaler        = 1U;
    hfdcan1.Init.DataSyncJumpWidth    = 4U;
    hfdcan1.Init.DataTimeSeg1         = 11U;
    hfdcan1.Init.DataTimeSeg2         = 4U;
    hfdcan1.Init.StdFiltersNbr        = 1U;
    hfdcan1.Init.ExtFiltersNbr        = 0U;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;   /* 넣은 순서대로 보냅니다 */
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }

    /* 받을 ID 두 개(명령 · 동기 신호)만 들여보내고 나머지는 버립니다 */
    FDCAN_FilterTypeDef f = {0};
    f.IdType       = FDCAN_STANDARD_ID;
    f.FilterIndex  = 0U;
    f.FilterType   = FDCAN_FILTER_DUAL;
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1    = SENSOR_CMD_ID;
    f.FilterID2    = SENSOR_SYNC_ID;
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &f) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_FDCAN_ConfigTxDelayCompensation(&hfdcan1, hfdcan1.Init.DataPrescaler * hfdcan1.Init.DataTimeSeg1, 0U) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_FDCAN_EnableTxDelayCompensation(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
}

/** @brief 초기화 실패 — LD2 를 빠르게 깜빡이며 멈춥니다(인터럽트는 끕니다) */
void Error_Handler(void)
{
    __disable_irq();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = LD2_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &gpio);
    for (;;) {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        for (volatile uint32_t i = 0U; i < 400000U; i++) {
        }
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
