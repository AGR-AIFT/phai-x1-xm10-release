/**
 *-----------------------------------------------------------
 *       BM1422AGMV MAGNETOMETER DRIVER REGISTER MAP
 *-----------------------------------------------------------
 * @file bm1422agmv_regmap.h
 * @date Created on: Jul 27, 2023
 * @author AngelRobotics HW
 * @brief Hardware Peripherals Register Map for BM1422AGMV.
 * 
 * This header file contains definitions for the BM1422AGMV magnetometer sensor.
 * It includes the device's sensitivity settings, device address, control register, 
 * and other constants required for interfacing with the sensor.
 * 
 * The BM1422AGMV is used to measure magnetic fields in three dimensions. 
 * It communicates over the I2C protocol and has specific sensitivity configurations 
 * for 12-bit and 14-bit measurements.
 * 
 * Refer to the BM1422AGMV datasheet for more detailed information on register
 * functions, configurations, and usage.
 *
 * @ref BM1422AGMV Datasheet
 */

#ifndef BM1422AGMV_INC_BM1422AGMV_REGMAP_H_
#define BM1422AGMV_INC_BM1422AGMV_REGMAP_H_

/* Device Specification */
#define BM1422AGMV_SENSITIVITY_14BIT 	24 			// Sensitivity for 14-bit measurement
#define BM1422AGMV_SENSITIVITY_12BIT 	6  			// Sensitivity for 12-bit measurement
#define BM1422AGMV_SCALE_FACTOR		 	0.042 		// 0.042uT/LSB

/* Device Address */
#define BM1422AGMV_DEV_ADDR_L		0b0001110 // Device low address
#define BM1422AGMV_DEV_ADDR_H		0b0001111 // Device high address
#define BM1422AGMV_DEV_MAX_HW_ID	0b0001    // Maximum hardware ID for the device
#define BM1422AGMV_DEV_ADDR			0x1C      // Device address
#define BM1422AGMV_CONTROL_SIZE		1U        // Control size in bytes
#define BM1422AGMV_CNTL4_REG_L		0x5CU     // Control register 4 low byte
#define BM1422AGMV_CNTL4_REG_H		0x5DU     // Control register 4 high byte
#define BM1422AGMV_CONTROL_SIZE		1U        // Control size in bytes (duplicate definition, consider removing)

// USED CNTL Register //
#define BM1422AGMV_CNTL1_VALUE 				0xD8U;	// 0xC0 : 14Bit(8192),10Hz,Continuous // 0x80 : 12Bit(2048),10Hz,Continuous // 0xD8 : 14Bit,1KHz,Continuous // 0xDA : 14Bit,1KHz,Single mode
//#define BM1422AGMV_CNTL2_VALUE 						0x0CU;
#define BM1422AGMV_CNTL2_VALUE 				0x00U;	// 0x00 : DRDY is not used // 0x08 : DRDY is used when the output is HIGH // 0x0C : DRDY is used when the output is LOW
#define BM1422AGMV_CNTL3_VALUE 				0x40U;	// Write "FORCE" = 1 for  measurement
#define BM1422AGMV_CNTL4_H_VALUE 			0x00U;
#define BM1422AGMV_CNTL4_L_VALUE 			0x00U;
#define BM1422AGMV_CTRL1_TEST				0xD8U;	//1KHz	// 0xD8 = 1101 1000 : 1KHz, Continuous mode, 0xDA = 1101 1010 : 1KHz, Single mode
//#define BM1422AGMV_CTRL1_TEST				0xC0U;	//10Hz
#define BM1422AGMV_CTRL2_TEST				0x0CU;	// 0x0C
#define BM1422AGMV_CTRL3_TEST				0x40U;
#define BM1422AGMV_CTRL4_H_TEST				0x00U;
#define BM1422AGMV_CTRL4_L_TEST				0x00U;
#define BM1422AGMV_SCALE_TEST				24.0f;
#define BM1422_DRDY							0x18U;
#define BM1422AGMV_INFO_LSB_REG         	0x0DU   // Information LSB  (default 0x01)
#define BM1422AGMV_INFO_MSB_REG         	0x0EU   // Information MSB  (default 0x01)
#define BM1422AGMV_WIA_REG              	0x0FU   // Who am I (default 0x41)
#define BM1422AGMV_DATAX_REG            	0x10U    // Xch Output Value LSB
#define BM1422AGMV_DATAY_REG            	0x12U    // Ych Output Value LSB
#define BM1422AGMV_DATAZ_REG            	0x14U    // Zch Output Value LSB
#define BM1422AGMV_STA_REG              	0x18U   // Status Register  (default 0x00)
#define BM1422AGMV_CNTL1_REG            	0x1BU   // Control Setting1 Register    (default 0x22)
#define BM1422AGMV_CNTL2_REG            	0x1CU   // Control Setting2 Register    (default 0x04)
#define BM1422AGMV_CNTL3_REG            	0x1DU   // Control Setting3 Register    (default 0x00)
#define BM1422AGMV_AVE_A_REG            	0x40U   // Average time Register    (default 0x00)
#define BM1422AGMV_CNTL4_REG            	0x5CU   // Control Setting4 Register    (default 0x04)
#define BM1422AGMV_TEMP_REG             	0x60U   // Temperature value LSB
#define BM1422AGMV_OFF_X_REG            	0x6CU   // Xch Offset Value     (default 0x30)
#define BM1422AGMV_OFF_Y_REG            	0x72U   // Ych Offset Value     (default 0x30)
#define BM1422AGMV_OFF_Z_REG            	0x78U   // Zch Offset Value     (default 0x30)
#define BM1422AGMV_FINEOUTPUTX_REG      	0x90U   // DATAX value per OFF_X LSB
#define BM1422AGMV_FINEOUTPUTY_REG      	0x92U   // DATAY value per OFF_Y LSB
#define BM1422AGMV_FINEOUTPUTZ_REG      	0x94U   // DATAZ value per OFF_Z LSB
#define BM1422AGMV_GAIN_PARA_X_TO_Z_REG 	0x9CU   // Axis interference Xch to Zch
#define BM1422AGMV_GAIN_PARA_X_TO_Y_REG 	0x9DU   // Axis interference Xch to Ych
#define BM1422AGMV_GAIN_PARA_Y_TO_Z_REG 	0x9EU   // Axis interference Ych to Zch
#define BM1422AGMV_GAIN_PARA_Y_TO_X_REG 	0x9FU   // Axis interference Ych to Xch
#define BM1422AGMV_GAIN_PARA_Z_TO_Y_REG 	0xA0U   // Axis interference Zch to Ych
#define BM1422AGMV_GAIN_PARA_Z_TO_X_REG 	0xA1U   // Axis interference Zch to Xch

/* Data Size */
#define BM1422AGMV_INFO_SIZE			2U
#define BM1422AGMV_DEV_SIZE				1U
#define BM1422AGMV_WIA_SIZE				1U
#define BM1422AGMV_DATAX_SIZE			2U
#define BM1422AGMV_DATAY_SIZE			2U
#define BM1422AGMV_DATAZ_SIZE			2U
#define BM1422AGMV_MAG_DATA_SIZE		6U
#define BM1422AGMV_STA1_SIZE			1U
#define BM1422AGMV_CNTL1_SIZE			1U
#define BM1422AGMV_CNTL2_SIZE			1U
#define BM1422AGMV_CNTL3_SIZE			1U
#define BM1422AGMV_AVE_A_SIZE			1U
#define BM1422AGMV_CNTL4_SIZE			2U
#define BM1422AGMV_TEMP_SIZE			2U
#define BM1422AGMV_OFF_X_SIZE			2U
#define BM1422AGMV_OFF_Y_SIZE			2U
#define BM1422AGMV_OFF_Z_SIZE			2U
#define BM1422AGMV_OFF_DATA_SIZE		6U
#define BM1422AGMV_FINEOUTPUTX_SIZE		2U
#define BM1422AGMV_FINEOUTPUTY_SIZE		2U
#define BM1422AGMV_FINEOUTPUTZ_SIZE		2U
#define BM1422AGMV_FINEOUTPUT_DATA_SIZE 6U
#define BM1422AGMV_GAIN_PARA_X_SIZE		2U
#define BM1422AGMV_GAIN_PARA_Y_SIZE		2U
#define BM1422AGMV_GAIN_PARA_Z_SIZE		2U
#define BM1422AGMV_GAIN_PARA_DATA_SIZE	6U

/* Register Data */
// Status Flag
#define BM1422AGMV_DATA_READY		0b01000000  // Ready OK
#define BM1422AGMV_DATA_NOT_READY	0b00000000  // Not Ready NG (default)

// Control 1 Configuration default 0x22 - 0010 0010
#define BM1422AGMV_POWER_CONTROL_DOWN	0b00000000  // Power Down
#define BM1422AGMV_POWER_CONTROL_ACTIVE	0b10000000  // Power Active
#define BM1422AGMV_OUT_BIT_12BIT		0b00000000  // Output Data setting 12bit Output
#define BM1422AGMV_OUT_BIT_14BIT		0b01000000  // Output Data setting 14bit Output
#define BM1422AGMV_RST_LV_RESET_RELEASE	0b00000000  // Reset release at RST_LV(CNTL1)=0 & RSTB_LV(CNT4)=1
#define BM1422AGMV_RST_LV_RESET			0b00100000  // Logic Reset Control - Reset
#define BM1422AGMV_ODR_10HZ				0b00000000  // Measurement Output Data Rates 10Hz
#define BM1422AGMV_ODR_20HZ				0b00010000  // Measurement Output Data Rates 20Hz
#define BM1422AGMV_ODR_100HZ			0b00001000  // Measurement Output Data Rates 100Hz
#define BM1422AGMV_ODR_1KHZ				0b00011000  // Measurement Output Data Rates 1KHz
#define BM1422AGMV_FS1_CONTINUOUS_MODE	0b00000000  // Measurement Mode Setting Continuous Mode
#define BM1422AGMV_FS1_SINGLE_MODE		0b00000010  // Measurement Mode Setting Single Mode
#define BM1422AGMV_CNTL4_VAL			0b00000000
#define BM1422AGMV_AVE_A_VAL			0b00000100	// 16 times

// Control 2 Configuration default 0x04 - 0000 0100
#define BM1422AGMV_DREN_DISABLE		0b00000000  // DRDY terminal enable setting disable
#define BM1422AGMV_DREN_ENABLE		0b00001000  // DRDY terminal enable setting enable
#define BM1422AGMV_DRP_LOW_ACTIVE	0b00000000  // DRDY terminal active setting Low active
#define BM1422AGMV_DRP_HIGH_ACTIVE	0b00000100  // DRDY terminal active setting High active

// Control 3 Configuration default 0x00 - 0000 0000
#define BM1422AGMV_FORCE	0b01000000  // AD Start Measurement trigger at continuous mode(FS=1) and single mode(FS=0)

// Control 4 Configuration default 0x04 - 0000 0100
#define BM1422AGMV_RSTB_LV_RESET_RELEASE	0b00000000  // Reset Release at RST_LV(CNT1)=0 & RSTB_LV(CNTL4)=1 / RSTB_LV=1 by write access (ignore write data)
																// RSTB_LV=0 by write PC1(CNTL1)=0
// Average time Configuration default 0x00 - 0000 0000
#define BM1422AGMV_AVE_A_4TIMES		0b00000000  // Average Time 4 times
#define BM1422AGMV_AVE_A_1TIMES		0b00000100  // Average Time 1 times
#define BM1422AGMV_AVE_A_2TIMES		0b00001000  // Average Time 2 times
#define BM1422AGMV_AVE_A_8TIMES		0b00001100  // Average Time 8 times
#define BM1422AGMV_AVE_A_16TIMES	0b00010000  // Average Time 16 times

#endif /* BM1422AGMV_INC_BM1422AGMV_REGMAP_H_ */
