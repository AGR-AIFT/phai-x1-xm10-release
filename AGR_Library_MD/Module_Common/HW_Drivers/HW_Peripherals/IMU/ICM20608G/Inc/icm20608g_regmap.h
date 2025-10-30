/**
 *-----------------------------------------------------------
 * ICM20608G ACCELEROMETER AND GYROSCOPE DRIVER REGISTER MAP
 *-----------------------------------------------------------
 * @file ICM20608G_REGMAP.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Register map definitions for ICM20608G IMU sensor.
 * 
 * This header file contains definitions for device addresses, register addresses,
 * data sizes, and configurations related to the ICM20608G sensor.
 * 
 * Refer to the ICM20608G datasheet and related documents for more information.
 *
 * @ref ICM20608G Datasheet
 */

#ifndef ICM20608G_INC_ICM20608G_REGMAP_H_
#define ICM20608G_INC_ICM20608G_REGMAP_H_

/* Device Address */
// Device address configurations for ICM20608G
#define ICM20608G_DEV_ADDR_LOW						0b1101000<<1	// Use Low value
#define ICM20608G_DEV_ADDR_HIGH						0b1101001<<1	// Use High value
#define ICM20608G_DEV_ADDR_TEST						0xD2U

// Specific configurations used by a particular application or mode
#define ICM20608G_PWR_MGMT_1_DATA				    0b00000000		// For 0x6B register
#define ICM20608G_PWR_MGMT_2_DATA				    0b00000000		// For 0x6C register
#define ICM20608G_GYR_CONFIG_250dps				    0b00000000		// For 0x1B register
#define ICM20608G_GYR_CONFIG_500dps				    0b00001000		// For 0x1B register
#define ICM20608G_GYR_CONFIG_1000dps				0b00010000		// For 0x1B register
#define ICM20608G_GYR_CONFIG_2000dps				0b00011000		// For 0x1B register
#define ICM20608G_ACC_CONFIG_2g				   		0b00000000		// For 0x1C register
#define ICM20608G_ACC_CONFIG_4g				    	0b00001000		// For 0x1C register
#define ICM20608G_ACC_CONFIG_8g				    	0b00010000		// For 0x1C register
#define ICM20608G_ACC_CONFIG_16g				    0b00011000		// For 0x1C register
#define ICM20608G_CONTROL_SIZE						1U

/* Register Address */
// Register address definitions for ICM20608G
// Includes offset, configuration, status, data, and control registers
#define ICM20608G_XG_OFFS_TC_H                    	0x04U   // Information MSB
#define ICM20608G_XG_OFFS_TC_L                   	0x05U   // Information LSB
#define ICM20608G_YG_OFFS_TC_H                    	0x07U   // Information MSB
#define ICM20608G_YG_OFFS_TC_L                   	0x08U   // Information LSB
#define ICM20608G_ZG_OFFS_TC_H                    	0x0AU   // Information MSB
#define ICM20608G_ZG_OFFS_TC_L                   	0x0BU   // Information LSB

#define ICM20608G_SELF_TEST_X_ACCEL                 0x0DU
#define ICM20608G_SELF_TEST_Y_ACCEL                 0x0EU
#define ICM20608G_SELF_TEST_Z_ACCEL                 0x0FU

#define ICM20608G_XG_OFFS_USRH						0x13U 	// Information MSB
#define ICM20608G_XG_OFFS_USRL						0x14U 	// Information LSB
#define ICM20608G_YG_OFFS_USRH						0x15U 	// Information MSB
#define ICM20608G_YG_OFFS_USRL						0x16U 	// Information LSB
#define ICM20608G_ZG_OFFS_USRH						0x17U 	// Information MSB
#define ICM20608G_ZG_OFFS_USRL						0x18U 	// Information LSB

#define ICM20608G_SMPLRT_DIV						0x19U

#define ICM20608G_CONFIG							0x1AU
#define ICM20608G_GYRO_CONFIG						0x1BU
#define ICM20608G_ACCEL_CONFIG						0x1CU
#define ICM20608G_ACCEL_CONFIG_2					0x1DU

#define ICM20608G_LP_MODE_CFG						0x1EU

#define ICM20608G_ACCEL_WOM_X_THR					0x20U
#define ICM20608G_ACCEL_WOM_Y_THR					0x21U
#define ICM20608G_ACCEL_WOM_Z_THR					0x22U

#define ICM20608G_FIFO_EN							0x23U
#define ICM20608G_FSYNC_INT							0x36U
#define ICM20608G_INT_PIN_CFG						0x37U
#define ICM20608G_INT_ENABLE						0x38U
#define ICM20608G_FIFO_WM_INT_STATUS				0x39U
#define ICM20608G_INT_STATUS						0x3AU

#define ICM20608G_ACCEL_XOUT_H						0x3BU	// Information MSB
#define ICM20608G_ACCEL_XOUT_L						0x3CU	// Information LSB
#define ICM20608G_ACCEL_YOUT_H						0x3DU	// Information MSB
#define ICM20608G_ACCEL_YOUT_L						0x3EU	// Information LSB
#define ICM20608G_ACCEL_ZOUT_H						0x3FU	// Information MSB
#define ICM20608G_ACCEL_ZOUT_L						0x40U	// Information LSB

#define ICM20608G_TEMP_OUT_H						0x41U	// Information MSB
#define ICM20608G_TEMP_OUT_L						0x42U	// Information LSB

#define ICM20608G_GYRO_XOUT_H						0x43U	// Information MSB
#define ICM20608G_GYRO_XOUT_L						0x44U	// Information LSB
#define ICM20608G_GYRO_YOUT_H						0x45U	// Information MSB
#define ICM20608G_GYRO_YOUT_L						0x46U	// Information LSB
#define ICM20608G_GYRO_ZOUT_H						0x47U	// Information MSB
#define ICM20608G_GYRO_ZOUT_L						0x48U	// Information LSB

#define ICM20608G_SELF_TEST_X_GYRO                  0x50U
#define ICM20608G_SELF_TEST_Y_GYRO                  0x51U
#define ICM20608G_SELF_TEST_Z_GYRO                  0x52U

#define ICM20608G_FIFO_WM_TH1						0x60U
#define ICM20608G_FIFO_WM_TH2						0x61U
#define ICM20608G_SIGNAL_PATH_RESET					0x68U
#define ICM20608G_ACCEL_INTEL_CTRL					0x69U
#define ICM20608G_USER_CTRL							0x6AU

#define ICM20608G_PWR_MGMT_1						0x6BU
#define ICM20608G_PWR_MGMT_2						0x6CU

#define ICM20608G_I2C_IF							0x70U
#define ICM20608G_FIFO_COUNTH						0x72U	// Information MSB
#define ICM20608G_FIFO_COUNTL						0x73U	// Information LSB
#define ICM20608G_FIFO_R_W							0x74U

#define ICM20608G_WHO_AM_I							0x75U

#define ICM20608G_XA_OFFSET_H						0x77U	// Information MSB
#define ICM20608G_XA_OFFSET_L						0x78U	// Information LSB
#define ICM20608G_YA_OFFSET_H						0x7AU	// Information MSB
#define ICM20608G_YA_OFFSET_L						0x7BU	// Information LSB
#define ICM20608G_ZA_OFFSET_H						0x7DU	// Information MSB
#define ICM20608G_ZA_OFFSET_L						0x7EU	// Information LSB

/* Data Size */
// Data size definitions for accelerometer, gyroscope, and configuration registers
#define ICM20608G_ACC_DATA_SIZE						6U
#define ICM20608G_GYR_DATA_SIZE						6U
#define ICM20608G_ACC_CONFIG1_SIZE					1U
#define ICM20608G_ACC_CONFIG2_SIZE					1U
#define ICM20608G_GYR_CONFIG_SIZE					1U

/* ACCEL Configuration */
// Accelerometer configuration values for different ranges and scales
#define ICM20608G_ACC_CONFIG_RANGE_2G          	 	0b00U	// Scale : 16384
#define ICM20608G_ACC_CONFIG_RANGE_4G           	0b01U	// Scale : 8192 (General)
#define ICM20608G_ACC_CONFIG_RANGE_8G           	0b10U	// Scale : 4096
#define ICM20608G_ACC_CONFIG_RANGE_16G          	0b11U	// Scale : 2048

/* GYRO Configuration */
// Gyroscope configuration values for different ranges and scales
#define ICM20608G_GYR_CONFIG_RANGE_250DPS        	0b00U	// Scale : 131
#define ICM20608G_GYR_CONFIG_RANGE_500DPS        	0b01U	// Scale : 65.5	(General)
#define ICM20608G_GYR_CONFIG_RANGE_1000DPS         	0b10U	// Scale : 32.8
#define ICM20608G_GYR_CONFIG_RANGE_2000DPS         	0b11U	// Scale : 16.4

#endif /* ICM20608G_INC_ICM20608G_REGMAP_H_ */
