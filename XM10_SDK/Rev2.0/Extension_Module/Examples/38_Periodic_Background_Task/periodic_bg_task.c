/**
 ******************************************************************************
 * @file    periodic_bg_task.c
 * @brief   Ex.38 — 100Hz Periodic Background Task + Mutex+Snapshot
 *
 * 학습 포인트
 *   1. XM_Task_CreatePeriodic() 로 보조 task 생성 (Control_Loop 와 동시 동작)
 *   2. 공유 변수 보호:
 *      - 단일 워드 (float / int / bool) → volatile 만으로 OK
 *      - 멀티 워드 (배열, 구조체)        → XM_Mutex_* 필수
 *   3. 1 kHz Writer (Control_Loop) 는 항상 timeout=0 — 실패 시 skip
 *   4. Reader (보조 task) 도 timeout=0 권장 — 실패 시 이전값 유지
 *
 * 시나리오
 *   - Control_Loop @1 kHz: ADC 샘플을 ring buffer (10개) 에 저장
 *   - AdcSummary @100 Hz : 10개 평균 계산 → 단일 float s_adc_avg 갱신
 *   - 사용자 알고리즘에서 s_adc_avg 만 읽으면 됨 (volatile 단일 워드)
 *
 * 회로 / 보드
 *   - Ex.04 Ext_IO_Basic 와 동일 — Analog CH1 입력 1개만 사용.
 ******************************************************************************
 */

#include "xm_api.h"

#define ADC_BUF_SIZE   10

static XmMutexHandle_t s_buf_mutex;
static XmTaskHandle_t  s_summary_task;

/* 단일 float — volatile 만으로 충분 (32-bit atomic on Cortex-M7) */
static volatile float  s_adc_avg;

/* 멀티 워드 배열 — Mutex 필수 */
static uint16_t        s_adc_buf[ADC_BUF_SIZE];
static int             s_adc_idx;

/* 100 Hz Reader — 10개 ADC 샘플 평균 계산 */
static void _SummaryTask(void)
{
    if (XM_Mutex_Lock(s_buf_mutex, 0U)) {       /* timeout=0 — 실패 시 skip */
        uint32_t sum = 0U;
        for (int i = 0; i < ADC_BUF_SIZE; ++i) sum += s_adc_buf[i];
        XM_Mutex_Unlock(s_buf_mutex);
        s_adc_avg = (float)sum / (float)ADC_BUF_SIZE;  /* 단일 워드 — lock 밖 OK */
    }
    /* Lock 실패 시 s_adc_avg 는 이전값 유지 (duplicate > garbage) */
}

void Control_Setup(void)
{
    s_buf_mutex    = XM_Mutex_Create();
    s_summary_task = XM_Task_CreatePeriodic("AdcSummary",
                                            _SummaryTask,
                                            10U,                /* 10 ms = 100 Hz */
                                            XM_PRIO_BACKGROUND);
}

void Control_Loop(void)
{
    /* 1 kHz Writer — Control_Loop 안에서는 항상 timeout=0 (절대 블로킹 금지).
     * 실패 시 이번 cycle 의 샘플은 건너뜀 (jitter 보존). */
    if (XM_Mutex_Lock(s_buf_mutex, 0U)) {
        s_adc_buf[s_adc_idx] = XM_AnalogRead(XM_EXT_ADC_1);   /* 0~4095 */
        s_adc_idx = (s_adc_idx + 1) % ADC_BUF_SIZE;
        XM_Mutex_Unlock(s_buf_mutex);
    }

    /* 단일 워드 read — lock 불필요. 사용자 알고리즘에서 그대로 사용 */
    float avg = s_adc_avg;
    (void)avg;
}
