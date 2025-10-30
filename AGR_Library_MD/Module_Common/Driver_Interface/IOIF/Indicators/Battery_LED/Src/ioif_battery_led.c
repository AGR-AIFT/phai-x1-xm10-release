

#include "ioif_battery_led.h"

/** @defgroup TIM TIM
  * @brief TIM Buzzer module driver
  * @{
  */
#ifdef IOIF_BATTERYLED_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */




/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */




/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */

static uint8_t batLedDmaRxBuff[IOIF_BATLED_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};

static PCA9531Object_t pca9531Obj;
static PCA9531IOctx_t pca9531IOctx;
static IOIF_I2C_t i2cHandle;


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static uint8_t IsDevReady(uint16_t devAddr, uint32_t trials, uint32_t timeout);
static uint8_t WriteReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */


/**
 * @brief Initialize PCA9531 Battery LED.
 * @param i2c Enumeration representing the I2C channel to use.
 * @return Status of the initialization operation.
 */
IOIF_BATLEDState_t IOIF_InitBatteryLED(IOIF_I2C_t i2c)
{
    pca9531IOctx.IsDevReady = IsDevReady;
    pca9531IOctx.WriteReg = WriteReg;
    
    i2cHandle = i2c;
    // for DMA Read
    pca9531Obj.pData = &batLedDmaRxBuff;

    uint8_t status = PCA9531_SetIoctx(&pca9531Obj, &pca9531IOctx);
    if (status != IOIF_BATLED_STATUS_OK) {
        return status; // Error handling
    }

    // Initialize the PCA9531 object
    // You should have a function for initialization
	status = PCA9531_Init(&pca9531Obj);
    if (status != IOIF_BATLED_STATUS_OK) {
        return status; // Error handling
    }

    IOIF_WriteGPIOPin(IOIF_GPIO_PORT_D, IOIF_GPIO_PIN_15, IOIF_GPIO_PIN_SET);

    return status;
}

/**
 * @brief Retrieve the current values from the PCA9531 Battery LED.
 * @param imuData Pointer to a structure to store the retrieved data.
 * @return Status of the data retrieval operation.
 */
IOIF_BATLEDState_t IOIF_RunBatteryLED(uint8_t ledOnBits)
{
    uint8_t status = 0;

	uint8_t tLsRegBuf0 = 0, tLsRegBuf1 = 0;
	uint8_t tLsReg0 = 0, tLsReg1 = 0;
	uint8_t tCompareBit = 0x01;

	tLsRegBuf0 = ledOnBits & 0x0f;
	tLsRegBuf1 = (ledOnBits & 0xf0) >> 4;

	tLsReg0 = 0;
	tLsReg1 = 0;
	tCompareBit = 0x01;

	for (int i = 0; i<4; i++) {
		tLsReg0 |= (tLsRegBuf0 & tCompareBit) << (i+1);
		tLsReg1 |= (tLsRegBuf1 & tCompareBit) << (i+1);

		tCompareBit = tCompareBit<< 1;
	}

	if (WriteReg(BATTERY_LED_ADDRESS, PCA9531_LS0_REG, &tLsReg0, 1) != IOIF_BATLED_STATUS_OK) { 
        status = IOIF_BATLED_STATUS_ERROR;
    }

	if (WriteReg(BATTERY_LED_ADDRESS, PCA9531_LS1_REG, &tLsReg1, 1) != IOIF_BATLED_STATUS_OK) { 
        status = IOIF_BATLED_STATUS_ERROR; 
    }

    return status;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

// Checks if the IMU 6-Axis device is ready
static uint8_t IsDevReady(uint16_t devAddr, uint32_t trials, uint32_t timeout)
{
    BSP_IsDevReadyI2C((BSP_I2C_t)i2cHandle, devAddr, trials, timeout); // trials = IOIF_BATLED_TRIALS, timeout = IOIF_BATLED_TIMEOUT

    //SET PSC0
	if (WriteReg(BATTERY_LED_ADDRESS, PCA9531_PSC0_REG, &(uint8_t){BATTERY_LED_OFF}, 1) != IOIF_BATLED_STATUS_OK) { return IOIF_BATLED_STATUS_ERROR; }

	//SET PSC1
	if (WriteReg(BATTERY_LED_ADDRESS, PCA9531_PSC1_REG, &(uint8_t){BATTERY_LED_OFF}, 1) != IOIF_BATLED_STATUS_OK) { return IOIF_BATLED_STATUS_ERROR; }

	//SET PWM0
	if (WriteReg(BATTERY_LED_ADDRESS, PCA9531_PWM0_REG, &(uint8_t){BATTERY_LED_PWM0}, 1) != IOIF_BATLED_STATUS_OK) { return IOIF_BATLED_STATUS_ERROR; }

	//SET PWM1
	if (WriteReg(BATTERY_LED_ADDRESS, PCA9531_PWM1_REG, &(uint8_t){BATTERY_LED_PWM1}, 1) != IOIF_BATLED_STATUS_OK) { return IOIF_BATLED_STATUS_ERROR; }

	return 0;
}

// Writes to a register on the IMU 6-Axis device using blocking method
static uint8_t WriteReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size)
{
	return BSP_I2CMemBlock((BSP_I2C_t)i2cHandle, devAddr, regAddr, BSP_I2C_MEMADD_SIZE_8BIT, pData, size, IOIF_BATLED_TIMEOUT, BSP_I2C_MEM_WRITE);
}


#endif /* IOIF_BATTERYLED_ENABLED */
