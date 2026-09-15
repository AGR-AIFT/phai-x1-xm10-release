/**
 * @file  task_mngr.h
 * @brief AGR_MW TSM public API — task/FSM lifecycle manager
 * @details
 *   This is the only public entry point for TSM. The internal FSM engine
 *   (task_state_machine.h) is private and lives under TSM/Inc/internal/.
 */

#ifndef AGR_MW_TSM_INC_TASK_MNGR_H_
#define AGR_MW_TSM_INC_TASK_MNGR_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 *                     TYPE DECLARATIONS
 *-----------------------------------------------------------
 */

/* State callbacks. */
typedef void (*EntryFunc_t)(void);
typedef void (*LoopFunc_t)(void);
typedef void (*ExitFunc_t)(void);

/* State lifecycle phase. */
typedef enum {
    TSM_LIFECYCLE_ENTRY,
    TSM_LIFECYCLE_LOOP,
    TSM_LIFECYCLE_EXIT
} TsmLifecycle_e;

/* Opaque task handle. Consumers receive a pointer from TaskMngr_Create()
 * and may only pass it back to TaskMngr_* APIs. Direct field access is
 * unavailable by design — use the getters below. */
typedef struct TsmObject_s TsmObject_t;

/**
 * @brief Tick provider for TsmDelay (RTOS / BareMetal neutral injection).
 *
 * Bind once at startup via TsmDelay_SetTickProvider(). Typical bindings:
 *   - FreeRTOS  : osKernelGetTickCount  [cmsis_os2.h]
 *   - BareMetal : HAL_GetTick           [STM32 HAL]
 *   - In-house  : IOIF_TIM_GetTick      [project-specific]
 *
 * Unit: milliseconds (caller is responsible for matching tick rate).
 */
typedef uint32_t (*TsmDelay_GetTickFunc_t)(void);

/**
 * @brief One-shot delay timer state — used by FSM Ent/Run pattern.
 *
 * POD struct held by the caller (typically as a static at file scope).
 * All fields are managed by TsmDelay_* — do not mutate directly.
 */
typedef struct TsmDelay_t {
    uint32_t start_ms;     /**< Tick recorded at TsmDelay_Start(). */
    uint32_t duration_ms;  /**< Configured duration. */
    bool     active;       /**< True between Start() and the firing edge. */
    bool     fired;        /**< True after the one-shot has reported expiry. */
} TsmDelay_t;

/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/** @brief Allocate a task from the static pool. Returns NULL if pool full. */
TsmObject_t* TaskMngr_Create(uint8_t initial_state_id);

/** @brief Register a state with Entry/Loop/Exit callbacks. */
void TaskMngr_AddState(TsmObject_t* obj, uint8_t state_id,
                       EntryFunc_t entry, LoopFunc_t loop, ExitFunc_t exit);

/** @brief Advance the task one tick. Call from a periodic context. */
void TaskMngr_Run(TsmObject_t* obj);

/** @brief Request a safe transition (exit current → enter next).
 *  @return 0 on success, -1 if state id not registered. */
int32_t TaskMngr_Transition(TsmObject_t* obj, uint8_t next_state_id);

/** @brief Reset the static pool (invalidates all outstanding handles). */
void TaskMngr_ResetPool(void);

/**
 *------------------------------------------------------------
 *                     STATE ACCESSORS
 *------------------------------------------------------------
 */

/** @brief Current state id, or 0xFF if obj is NULL. */
uint8_t TaskMngr_GetStateId(const TsmObject_t* obj);

/** @brief Current lifecycle phase, or TSM_LIFECYCLE_ENTRY if obj is NULL. */
TsmLifecycle_e TaskMngr_GetLifecycle(const TsmObject_t* obj);

/** @brief Previous state id, or 0xFF if obj is NULL. */
uint8_t TaskMngr_GetPrevStateId(const TsmObject_t* obj);

/** @brief Previous lifecycle phase, or TSM_LIFECYCLE_ENTRY if obj is NULL. */
TsmLifecycle_e TaskMngr_GetPrevLifecycle(const TsmObject_t* obj);

/**
 *------------------------------------------------------------
 *                     TsmDelay — One-shot Timer
 *------------------------------------------------------------
 * @details
 *   Convenience timer for the FSM Ent/Run pattern: arm in the state's
 *   Entry callback, poll in Loop, transition on expiry. Avoids osDelay()
 *   blocking the host task and removes wrap-around boilerplate from
 *   every caller.
 *
 *   RTOS-neutral: caller injects the tick source via
 *   TsmDelay_SetTickProvider() at startup. No direct dependency on
 *   cmsis_os2.h or HAL — preserves AGR_MW's RTOS/BareMetal portability.
 */

/**
 * @brief Bind the global tick source. Call once at startup.
 *
 * Must be called before any TsmDelay_Start(). Until set, TsmDelay_Expired()
 * returns false (timer never fires) and TsmDelay_Start() leaves the timer
 * inactive.
 *
 * @param fn Tick getter. Pass NULL to clear the binding (e.g. for tests).
 */
void TsmDelay_SetTickProvider(TsmDelay_GetTickFunc_t fn);

/**
 * @brief Arm a one-shot delay. Call from the state's Entry callback.
 *
 * Records the current tick and resets the fired flag. Re-calling Start()
 * re-arms the timer regardless of prior state.
 *
 * @param d           Delay handle (caller-owned). NULL is a no-op.
 * @param duration_ms Duration in milliseconds. Must be <= INT32_MAX
 *                    for wrap-safe comparison (~24.8 days at 1 kHz).
 *
 * @note If no tick provider is bound, the timer is left inactive
 *       (TsmDelay_Expired() will return false until provider is set
 *       AND Start() is called again).
 */
void TsmDelay_Start(TsmDelay_t* d, uint32_t duration_ms);

/**
 * @brief Has the armed delay elapsed? Call from the state's Loop callback.
 *
 * One-shot semantics — returns true exactly once per Start() call. Subsequent
 * polls return false until the next Start(). Use TsmDelay_Reset() if you need
 * to abort an armed delay without firing.
 *
 * @param d Delay handle. NULL is treated as not-expired.
 * @return true exactly once on the firing edge; false otherwise.
 *
 * @note Single-shot. Re-arm via TsmDelay_Start().
 */
bool TsmDelay_Expired(TsmDelay_t* d);

/**
 * @brief Disarm a delay without firing. Idempotent.
 *
 * Normally unnecessary — TsmDelay_Start() already re-arms. Use when a state
 * Exit must cancel a delay armed in Entry but not yet expired.
 *
 * @param d Delay handle. NULL is a no-op.
 */
void TsmDelay_Reset(TsmDelay_t* d);

#endif /* AGR_MW_TSM_INC_TASK_MNGR_H_ */
