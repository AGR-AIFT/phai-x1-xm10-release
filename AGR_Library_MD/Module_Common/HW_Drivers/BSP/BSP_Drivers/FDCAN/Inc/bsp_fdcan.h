/**
 * @file bsp_fdcan.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for FDCAN functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef FDCAN_INC_BSP_FDCAN_H_
#define FDCAN_INC_BSP_FDCAN_H_

#include "main.h"
#include "module.h"

/** @defgroup FDCAN FDCAN
  * @brief FDCAN HAL BSP module driver
  * @
  */
#ifdef HAL_FDCAN_MODULE_ENABLED
#define BSP_FDCAN_MODULE_ENABLED

#include "fdcan.h"
#include "bsp_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/** @defgroup FDCAN_data_field_size FDCAN Data Field Size
  * @{
  */
#define BSP_FDCAN_DATA_BYTES_8  FDCAN_DATA_BYTES_8   /*!< 8 bytes data field  */
#define BSP_FDCAN_DATA_BYTES_12 FDCAN_DATA_BYTES_12  /*!< 12 bytes data field */
#define BSP_FDCAN_DATA_BYTES_16 FDCAN_DATA_BYTES_16  /*!< 16 bytes data field */
#define BSP_FDCAN_DATA_BYTES_20 FDCAN_DATA_BYTES_20  /*!< 20 bytes data field */
#define BSP_FDCAN_DATA_BYTES_24 FDCAN_DATA_BYTES_24  /*!< 24 bytes data field */
#define BSP_FDCAN_DATA_BYTES_32 FDCAN_DATA_BYTES_32  /*!< 32 bytes data field */
#define BSP_FDCAN_DATA_BYTES_48 FDCAN_DATA_BYTES_48  /*!< 48 bytes data field */
#define BSP_FDCAN_DATA_BYTES_64 FDCAN_DATA_BYTES_64  /*!< 64 bytes data field */

/** @defgroup FDCAN_txFifoQueue_Mode FDCAN Tx FIFO/Queue Mode
  * @{
  */
#define BSP_FDCAN_TX_FIFO_OPERATION  FDCAN_TX_FIFO_OPERATION   /*!< FIFO mode  */
#define BSP_FDCAN_TX_QUEUE_OPERATION FDCAN_TX_QUEUE_OPERATION  /*!< Queue mode */

/** @defgroup FDCAN_id_type FDCAN ID Type
  * @{
  */
#define BSP_FDCAN_STANDARD_ID FDCAN_STANDARD_ID  /*!< Standard ID element */
#define BSP_FDCAN_EXTENDED_ID FDCAN_EXTENDED_ID  /*!< Extended ID element */

/** @defgroup FDCAN_frame_type FDCAN Frame Type
  * @{
  */
#define BSP_FDCAN_DATA_FRAME   FDCAN_DATA_FRAME    /*!< Data frame   */
#define BSP_FDCAN_REMOTE_FRAME FDCAN_REMOTE_FRAME  /*!< Remote frame */

/** @defgroup FDCAN_data_length_code FDCAN Data Length Code
  * @{
  */
#define BSP_FDCAN_DLC_BYTES_0  FDCAN_DLC_BYTES_0   /*!< 0 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_1  FDCAN_DLC_BYTES_1   /*!< 1 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_2  FDCAN_DLC_BYTES_2   /*!< 2 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_3  FDCAN_DLC_BYTES_3   /*!< 3 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_4  FDCAN_DLC_BYTES_4   /*!< 4 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_5  FDCAN_DLC_BYTES_5   /*!< 5 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_6  FDCAN_DLC_BYTES_6   /*!< 6 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_7  FDCAN_DLC_BYTES_7   /*!< 7 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_8  FDCAN_DLC_BYTES_8   /*!< 8 bytes data field  */
#define BSP_FDCAN_DLC_BYTES_12 FDCAN_DLC_BYTES_12  /*!< 12 bytes data field */
#define BSP_FDCAN_DLC_BYTES_16 FDCAN_DLC_BYTES_16  /*!< 16 bytes data field */
#define BSP_FDCAN_DLC_BYTES_20 FDCAN_DLC_BYTES_20  /*!< 20 bytes data field */
#define BSP_FDCAN_DLC_BYTES_24 FDCAN_DLC_BYTES_24  /*!< 24 bytes data field */
#define BSP_FDCAN_DLC_BYTES_32 FDCAN_DLC_BYTES_32  /*!< 32 bytes data field */
#define BSP_FDCAN_DLC_BYTES_48 FDCAN_DLC_BYTES_48  /*!< 48 bytes data field */
#define BSP_FDCAN_DLC_BYTES_64 FDCAN_DLC_BYTES_64  /*!< 64 bytes data field */

/** @defgroup FDCAN_error_state_indicator FDCAN Error State Indicator
  * @{
  */
#define BSP_FDCAN_ESI_ACTIVE  FDCAN_ESI_ACTIVE   /*!< Transmitting node is error active  */
#define BSP_FDCAN_ESI_PASSIVE FDCAN_ESI_PASSIVE  /*!< Transmitting node is error passive */

/** @defgroup FDCAN_bit_rate_switching FDCAN Bit Rate Switching
  * @{
  */
#define BSP_FDCAN_BRS_OFF FDCAN_BRS_OFF  /*!< FDCAN frames transmitted/received without bit rate switching */
#define BSP_FDCAN_BRS_ON  FDCAN_BRS_ON   /*!< FDCAN frames transmitted/received with bit rate switching    */

/** @defgroup FDCAN_format FDCAN format
  * @{
  */
#define BSP_FDCAN_CLASSIC_CAN FDCAN_CLASSIC_CAN  /*!< Frame transmitted/received in Classic CAN format */
#define BSP_FDCAN_FD_CAN      FDCAN_FD_CAN       /*!< Frame transmitted/received in FDCAN format       */

/** @defgroup FDCAN_EFC FDCAN Event FIFO control
  * @{
  */
#define BSP_FDCAN_NO_TX_EVENTS    FDCAN_NO_TX_EVENTS     /*!< Do not store Tx events */
#define BSP_FDCAN_STORE_TX_EVENTS FDCAN_STORE_TX_EVENTS  /*!< Store Tx events        */

/** @defgroup FDCAN_filter_type FDCAN Filter Type
  * @{
  */
#define BSP_FDCAN_FILTER_RANGE         FDCAN_FILTER_RANGE          /*!< Range filter from FilterID1 to FilterID2                        */
#define BSP_FDCAN_FILTER_DUAL          FDCAN_FILTER_DUAL           /*!< Dual ID filter for FilterID1 or FilterID2                       */
#define BSP_FDCAN_FILTER_MASK          FDCAN_FILTER_MASK           /*!< Classic filter: FilterID1 = filter, FilterID2 = mask            */
#define BSP_FDCAN_FILTER_RANGE_NO_EIDM FDCAN_FILTER_RANGE_NO_EIDM  /*!< Range filter from FilterID1 to FilterID2, EIDM mask not applied */

/** @defgroup FDCAN_filter_config FDCAN Filter Configuration
  * @{
  */
#define BSP_FDCAN_FILTER_DISABLE       FDCAN_FILTER_DISABLE        /*!< Disable filter element                                    */
#define BSP_FDCAN_FILTER_TO_RXFIFO0    FDCAN_FILTER_TO_RXFIFO0     /*!< Store in Rx FIFO 0 if filter matches                      */
#define BSP_FDCAN_FILTER_TO_RXFIFO1    FDCAN_FILTER_TO_RXFIFO1     /*!< Store in Rx FIFO 1 if filter matches                      */
#define BSP_FDCAN_FILTER_REJECT        FDCAN_FILTER_REJECT         /*!< Reject ID if filter matches                               */
#define BSP_FDCAN_FILTER_HP            FDCAN_FILTER_HP             /*!< Set high priority if filter matches                       */
#define BSP_FDCAN_FILTER_TO_RXFIFO0_HP FDCAN_FILTER_TO_RXFIFO0_HP  /*!< Set high priority and store in FIFO 0 if filter matches   */
#define BSP_FDCAN_FILTER_TO_RXFIFO1_HP FDCAN_FILTER_TO_RXFIFO1_HP  /*!< Set high priority and store in FIFO 1 if filter matches   */
#define BSP_FDCAN_FILTER_TO_RXBUFFER   FDCAN_FILTER_TO_RXBUFFER    /*!< Store into Rx Buffer, configuration of FilterType ignored */

/** @defgroup FDCAN_Tx_location FDCAN Tx Location
  * @{
  */
#define BSP_FDCAN_TX_BUFFER0  FDCAN_TX_BUFFER0   /*!< Add message to Tx Buffer 0  */
#define BSP_FDCAN_TX_BUFFER1  FDCAN_TX_BUFFER1   /*!< Add message to Tx Buffer 1  */
#define BSP_FDCAN_TX_BUFFER2  FDCAN_TX_BUFFER2   /*!< Add message to Tx Buffer 2  */
#define BSP_FDCAN_TX_BUFFER3  FDCAN_TX_BUFFER3   /*!< Add message to Tx Buffer 3  */
#define BSP_FDCAN_TX_BUFFER4  FDCAN_TX_BUFFER4   /*!< Add message to Tx Buffer 4  */
#define BSP_FDCAN_TX_BUFFER5  FDCAN_TX_BUFFER5   /*!< Add message to Tx Buffer 5  */
#define BSP_FDCAN_TX_BUFFER6  FDCAN_TX_BUFFER6   /*!< Add message to Tx Buffer 6  */
#define BSP_FDCAN_TX_BUFFER7  FDCAN_TX_BUFFER7   /*!< Add message to Tx Buffer 7  */
#define BSP_FDCAN_TX_BUFFER8  FDCAN_TX_BUFFER8   /*!< Add message to Tx Buffer 8  */
#define BSP_FDCAN_TX_BUFFER9  FDCAN_TX_BUFFER9   /*!< Add message to Tx Buffer 9  */
#define BSP_FDCAN_TX_BUFFER10 FDCAN_TX_BUFFER1   /*!< Add message to Tx Buffer 10 */
#define BSP_FDCAN_TX_BUFFER11 FDCAN_TX_BUFFER11  /*!< Add message to Tx Buffer 11 */
#define BSP_FDCAN_TX_BUFFER12 FDCAN_TX_BUFFER12  /*!< Add message to Tx Buffer 12 */
#define BSP_FDCAN_TX_BUFFER13 FDCAN_TX_BUFFER13  /*!< Add message to Tx Buffer 13 */
#define BSP_FDCAN_TX_BUFFER14 FDCAN_TX_BUFFER14  /*!< Add message to Tx Buffer 14 */
#define BSP_FDCAN_TX_BUFFER15 FDCAN_TX_BUFFER15  /*!< Add message to Tx Buffer 15 */
#define BSP_FDCAN_TX_BUFFER16 FDCAN_TX_BUFFER16  /*!< Add message to Tx Buffer 16 */
#define BSP_FDCAN_TX_BUFFER17 FDCAN_TX_BUFFER17  /*!< Add message to Tx Buffer 17 */
#define BSP_FDCAN_TX_BUFFER18 FDCAN_TX_BUFFER18  /*!< Add message to Tx Buffer 18 */
#define BSP_FDCAN_TX_BUFFER19 FDCAN_TX_BUFFER19  /*!< Add message to Tx Buffer 19 */
#define BSP_FDCAN_TX_BUFFER20 FDCAN_TX_BUFFER20  /*!< Add message to Tx Buffer 20 */
#define BSP_FDCAN_TX_BUFFER21 FDCAN_TX_BUFFER21  /*!< Add message to Tx Buffer 21 */
#define BSP_FDCAN_TX_BUFFER22 FDCAN_TX_BUFFER22  /*!< Add message to Tx Buffer 22 */
#define BSP_FDCAN_TX_BUFFER23 FDCAN_TX_BUFFER23  /*!< Add message to Tx Buffer 23 */
#define BSP_FDCAN_TX_BUFFER24 FDCAN_TX_BUFFER24  /*!< Add message to Tx Buffer 24 */
#define BSP_FDCAN_TX_BUFFER25 FDCAN_TX_BUFFER25  /*!< Add message to Tx Buffer 25 */
#define BSP_FDCAN_TX_BUFFER26 FDCAN_TX_BUFFER26  /*!< Add message to Tx Buffer 26 */
#define BSP_FDCAN_TX_BUFFER27 FDCAN_TX_BUFFER27  /*!< Add message to Tx Buffer 27 */
#define BSP_FDCAN_TX_BUFFER28 FDCAN_TX_BUFFER28  /*!< Add message to Tx Buffer 28 */
#define BSP_FDCAN_TX_BUFFER29 FDCAN_TX_BUFFER29  /*!< Add message to Tx Buffer 29 */
#define BSP_FDCAN_TX_BUFFER30 FDCAN_TX_BUFFER30  /*!< Add message to Tx Buffer 30 */
#define BSP_FDCAN_TX_BUFFER31 FDCAN_TX_BUFFER31  /*!< Add message to Tx Buffer 31 */

/** @defgroup FDCAN_Rx_location FDCAN Rx Location
  * @{
  */
#define BSP_FDCAN_RX_FIFO0    FDCAN_RX_FIFO0     /*!< Get received message from Rx FIFO 0    */
#define BSP_FDCAN_RX_FIFO1    FDCAN_RX_FIFO1     /*!< Get received message from Rx FIFO 1    */
#define BSP_FDCAN_RX_BUFFER0  FDCAN_RX_BUFFER0   /*!< Get received message from Rx Buffer 0  */
#define BSP_FDCAN_RX_BUFFER1  FDCAN_RX_BUFFER1   /*!< Get received message from Rx Buffer 1  */
#define BSP_FDCAN_RX_BUFFER2  FDCAN_RX_BUFFER2   /*!< Get received message from Rx Buffer 2  */
#define BSP_FDCAN_RX_BUFFER3  FDCAN_RX_BUFFER3   /*!< Get received message from Rx Buffer 3  */
#define BSP_FDCAN_RX_BUFFER4  FDCAN_RX_BUFFER4   /*!< Get received message from Rx Buffer 4  */
#define BSP_FDCAN_RX_BUFFER5  FDCAN_RX_BUFFER5   /*!< Get received message from Rx Buffer 5  */
#define BSP_FDCAN_RX_BUFFER6  FDCAN_RX_BUFFER6   /*!< Get received message from Rx Buffer 6  */
#define BSP_FDCAN_RX_BUFFER7  FDCAN_RX_BUFFER7   /*!< Get received message from Rx Buffer 7  */
#define BSP_FDCAN_RX_BUFFER8  FDCAN_RX_BUFFER8   /*!< Get received message from Rx Buffer 8  */
#define BSP_FDCAN_RX_BUFFER9  FDCAN_RX_BUFFER9   /*!< Get received message from Rx Buffer 9  */
#define BSP_FDCAN_RX_BUFFER10 FDCAN_RX_BUFFER10  /*!< Get received message from Rx Buffer 10 */
#define BSP_FDCAN_RX_BUFFER11 FDCAN_RX_BUFFER11  /*!< Get received message from Rx Buffer 11 */
#define BSP_FDCAN_RX_BUFFER12 FDCAN_RX_BUFFER12  /*!< Get received message from Rx Buffer 12 */
#define BSP_FDCAN_RX_BUFFER13 FDCAN_RX_BUFFER13  /*!< Get received message from Rx Buffer 13 */
#define BSP_FDCAN_RX_BUFFER14 FDCAN_RX_BUFFER14  /*!< Get received message from Rx Buffer 14 */
#define BSP_FDCAN_RX_BUFFER15 FDCAN_RX_BUFFER15  /*!< Get received message from Rx Buffer 15 */
#define BSP_FDCAN_RX_BUFFER16 FDCAN_RX_BUFFER16  /*!< Get received message from Rx Buffer 16 */
#define BSP_FDCAN_RX_BUFFER17 FDCAN_RX_BUFFER17  /*!< Get received message from Rx Buffer 17 */
#define BSP_FDCAN_RX_BUFFER18 FDCAN_RX_BUFFER18  /*!< Get received message from Rx Buffer 18 */
#define BSP_FDCAN_RX_BUFFER19 FDCAN_RX_BUFFER19  /*!< Get received message from Rx Buffer 19 */
#define BSP_FDCAN_RX_BUFFER20 FDCAN_RX_BUFFER20  /*!< Get received message from Rx Buffer 20 */
#define BSP_FDCAN_RX_BUFFER21 FDCAN_RX_BUFFER21  /*!< Get received message from Rx Buffer 21 */
#define BSP_FDCAN_RX_BUFFER22 FDCAN_RX_BUFFER22  /*!< Get received message from Rx Buffer 22 */
#define BSP_FDCAN_RX_BUFFER23 FDCAN_RX_BUFFER23  /*!< Get received message from Rx Buffer 23 */
#define BSP_FDCAN_RX_BUFFER24 FDCAN_RX_BUFFER24  /*!< Get received message from Rx Buffer 24 */
#define BSP_FDCAN_RX_BUFFER25 FDCAN_RX_BUFFER25  /*!< Get received message from Rx Buffer 25 */
#define BSP_FDCAN_RX_BUFFER26 FDCAN_RX_BUFFER26  /*!< Get received message from Rx Buffer 26 */
#define BSP_FDCAN_RX_BUFFER27 FDCAN_RX_BUFFER27  /*!< Get received message from Rx Buffer 27 */
#define BSP_FDCAN_RX_BUFFER28 FDCAN_RX_BUFFER28  /*!< Get received message from Rx Buffer 28 */
#define BSP_FDCAN_RX_BUFFER29 FDCAN_RX_BUFFER29  /*!< Get received message from Rx Buffer 29 */
#define BSP_FDCAN_RX_BUFFER30 FDCAN_RX_BUFFER30  /*!< Get received message from Rx Buffer 30 */
#define BSP_FDCAN_RX_BUFFER31 FDCAN_RX_BUFFER31  /*!< Get received message from Rx Buffer 31 */
#define BSP_FDCAN_RX_BUFFER32 FDCAN_RX_BUFFER32  /*!< Get received message from Rx Buffer 32 */
#define BSP_FDCAN_RX_BUFFER33 FDCAN_RX_BUFFER33  /*!< Get received message from Rx Buffer 33 */
#define BSP_FDCAN_RX_BUFFER34 FDCAN_RX_BUFFER34  /*!< Get received message from Rx Buffer 34 */
#define BSP_FDCAN_RX_BUFFER35 FDCAN_RX_BUFFER35  /*!< Get received message from Rx Buffer 35 */
#define BSP_FDCAN_RX_BUFFER36 FDCAN_RX_BUFFER36  /*!< Get received message from Rx Buffer 36 */
#define BSP_FDCAN_RX_BUFFER37 FDCAN_RX_BUFFER37  /*!< Get received message from Rx Buffer 37 */
#define BSP_FDCAN_RX_BUFFER38 FDCAN_RX_BUFFER38  /*!< Get received message from Rx Buffer 38 */
#define BSP_FDCAN_RX_BUFFER39 FDCAN_RX_BUFFER39  /*!< Get received message from Rx Buffer 39 */
#define BSP_FDCAN_RX_BUFFER40 FDCAN_RX_BUFFER40  /*!< Get received message from Rx Buffer 40 */
#define BSP_FDCAN_RX_BUFFER41 FDCAN_RX_BUFFER41  /*!< Get received message from Rx Buffer 41 */
#define BSP_FDCAN_RX_BUFFER42 FDCAN_RX_BUFFER42  /*!< Get received message from Rx Buffer 42 */
#define BSP_FDCAN_RX_BUFFER43 FDCAN_RX_BUFFER43  /*!< Get received message from Rx Buffer 43 */
#define BSP_FDCAN_RX_BUFFER44 FDCAN_RX_BUFFER44  /*!< Get received message from Rx Buffer 44 */
#define BSP_FDCAN_RX_BUFFER45 FDCAN_RX_BUFFER45  /*!< Get received message from Rx Buffer 45 */
#define BSP_FDCAN_RX_BUFFER46 FDCAN_RX_BUFFER46  /*!< Get received message from Rx Buffer 46 */
#define BSP_FDCAN_RX_BUFFER47 FDCAN_RX_BUFFER47  /*!< Get received message from Rx Buffer 47 */
#define BSP_FDCAN_RX_BUFFER48 FDCAN_RX_BUFFER48  /*!< Get received message from Rx Buffer 48 */
#define BSP_FDCAN_RX_BUFFER49 FDCAN_RX_BUFFER49  /*!< Get received message from Rx Buffer 49 */
#define BSP_FDCAN_RX_BUFFER50 FDCAN_RX_BUFFER50  /*!< Get received message from Rx Buffer 50 */
#define BSP_FDCAN_RX_BUFFER51 FDCAN_RX_BUFFER51  /*!< Get received message from Rx Buffer 51 */
#define BSP_FDCAN_RX_BUFFER52 FDCAN_RX_BUFFER52  /*!< Get received message from Rx Buffer 52 */
#define BSP_FDCAN_RX_BUFFER53 FDCAN_RX_BUFFER53  /*!< Get received message from Rx Buffer 53 */
#define BSP_FDCAN_RX_BUFFER54 FDCAN_RX_BUFFER54  /*!< Get received message from Rx Buffer 54 */
#define BSP_FDCAN_RX_BUFFER55 FDCAN_RX_BUFFER55  /*!< Get received message from Rx Buffer 55 */
#define BSP_FDCAN_RX_BUFFER56 FDCAN_RX_BUFFER56  /*!< Get received message from Rx Buffer 56 */
#define BSP_FDCAN_RX_BUFFER57 FDCAN_RX_BUFFER57  /*!< Get received message from Rx Buffer 57 */
#define BSP_FDCAN_RX_BUFFER58 FDCAN_RX_BUFFER58  /*!< Get received message from Rx Buffer 58 */
#define BSP_FDCAN_RX_BUFFER59 FDCAN_RX_BUFFER59  /*!< Get received message from Rx Buffer 59 */
#define BSP_FDCAN_RX_BUFFER60 FDCAN_RX_BUFFER60  /*!< Get received message from Rx Buffer 60 */
#define BSP_FDCAN_RX_BUFFER61 FDCAN_RX_BUFFER61  /*!< Get received message from Rx Buffer 61 */
#define BSP_FDCAN_RX_BUFFER62 FDCAN_RX_BUFFER62  /*!< Get received message from Rx Buffer 62 */
#define BSP_FDCAN_RX_BUFFER63 FDCAN_RX_BUFFER63  /*!< Get received message from Rx Buffer 63 */

/** @defgroup FDCAN_event_type FDCAN Event Type
  * @{
  */
#define BSP_FDCAN_TX_EVENT             FDCAN_TX_EVENT              /*!< Tx event                              */
#define BSP_FDCAN_TX_IN_SPITE_OF_ABORT FDCAN_TX_IN_SPITE_OF_ABORT  /*!< Transmission in spite of cancellation */

/** @defgroup FDCAN_protocol_error_code FDCAN protocol error code
  * @{
  */
#define BSP_FDCAN_PROTOCOL_ERROR_NONE      FDCAN_PROTOCOL_ERROR_NONE       /*!< No error occurred         */
#define BSP_FDCAN_PROTOCOL_ERROR_STUFF     FDCAN_PROTOCOL_ERROR_STUFF      /*!< Stuff error               */
#define BSP_FDCAN_PROTOCOL_ERROR_FORM      FDCAN_PROTOCOL_ERROR_FORM       /*!< Form error                */
#define BSP_FDCAN_PROTOCOL_ERROR_ACK       FDCAN_PROTOCOL_ERROR_ACK        /*!< Acknowledge error         */
#define BSP_FDCAN_PROTOCOL_ERROR_BIT1      FDCAN_PROTOCOL_ERROR_BIT1       /*!< Bit 1 (recessive) error   */
#define BSP_FDCAN_PROTOCOL_ERROR_BIT0      FDCAN_PROTOCOL_ERROR_BIT0       /*!< Bit 0 (dominant) error    */
#define BSP_FDCAN_PROTOCOL_ERROR_CRC       FDCAN_PROTOCOL_ERROR_CRC        /*!< CRC check sum error       */
#define BSP_FDCAN_PROTOCOL_ERROR_NO_CHANGE FDCAN_PROTOCOL_ERROR_NO_CHANGE  /*!< No change since last read */

/** @defgroup FDCAN_communication_state FDCAN communication state
  * @{
  */
#define BSP_FDCAN_COM_STATE_SYNC FDCAN_COM_STATE_SYNC  /*!< Node is synchronizing on CAN communication */
#define BSP_FDCAN_COM_STATE_IDLE FDCAN_COM_STATE_IDLE  /*!< Node is neither receiver nor transmitter   */
#define BSP_FDCAN_COM_STATE_RX   FDCAN_COM_STATE_RX    /*!< Node is operating as receiver              */
#define BSP_FDCAN_COM_STATE_TX   FDCAN_COM_STATE_TX    /*!< Node is operating as transmitter           */

/** @defgroup FDCAN_Rx_FIFO_operation_mode FDCAN FIFO operation mode
  * @{
  */
#define BSP_FDCAN_RX_FIFO_BLOCKING  FDCAN_RX_FIFO_BLOCKING   /*!< Rx FIFO blocking mode  */
#define BSP_FDCAN_RX_FIFO_OVERWRITE FDCAN_RX_FIFO_OVERWRITE  /*!< Rx FIFO overwrite mode */

/** @defgroup FDCAN_Non_Matching_Frames FDCAN non-matching frames
  * @{
  */
#define BSP_FDCAN_ACCEPT_IN_RX_FIFO0 FDCAN_ACCEPT_IN_RX_FIFO0  /*!< Accept in Rx FIFO 0 */
#define BSP_FDCAN_ACCEPT_IN_RX_FIFO1 FDCAN_ACCEPT_IN_RX_FIFO1  /*!< Accept in Rx FIFO 1 */
#define BSP_FDCAN_REJECT             FDCAN_REJECT              /*!< Reject              */

/** @defgroup FDCAN_Reject_Remote_Frames FDCAN reject remote frames
  * @{
  */
#define BSP_FDCAN_FILTER_REMOTE FDCAN_FILTER_REMOTE  /*!< Filter remote frames */
#define BSP_FDCAN_REJECT_REMOTE FDCAN_REJECT_REMOTE  /*!< Reject all remote frames */

/** @defgroup FDCAN_Interrupt_Line FDCAN interrupt line
  * @{
  */
#define BSP_FDCAN_INTERRUPT_LINE0 FDCAN_INTERRUPT_LINE0  /*!< Interrupt Line 0 */
#define BSP_FDCAN_INTERRUPT_LINE1 FDCAN_INTERRUPT_LINE1  /*!< Interrupt Line 1 */

/** @defgroup FDCAN_Timeout_Operation FDCAN timeout operation
  * @{
  */
#define BSP_FDCAN_TIMEOUT_CONTINUOUS    FDCAN_TIMEOUT_CONTINUOUS    /*!< Timeout continuous operation        */
#define BSP_FDCAN_TIMEOUT_TX_EVENT_FIFO FDCAN_TIMEOUT_TX_EVENT_FIFO /*!< Timeout controlled by Tx Event FIFO */
#define BSP_FDCAN_TIMEOUT_RX_FIFO0      FDCAN_TIMEOUT_RX_FIFO0      /*!< Timeout controlled by Rx FIFO 0     */
#define BSP_FDCAN_TIMEOUT_RX_FIFO1      FDCAN_TIMEOUT_RX_FIFO1      /*!< Timeout controlled by Rx FIFO 1     */

/** @defgroup FDCAN_Rx_Fifo0_Interrupts FDCAN Rx FIFO 0 Interrupts
  * @{
  */
#define BSP_FDCAN_IT_RX_FIFO0_MESSAGE_LOST FDCAN_IT_RX_FIFO0_MESSAGE_LOST  /*!< Rx FIFO 0 message lost                 */
#define BSP_FDCAN_IT_RX_FIFO0_FULL         FDCAN_IT_RX_FIFO0_FULL          /*!< Rx FIFO 0 full                         */
#define BSP_FDCAN_IT_RX_FIFO0_WATERMARK    FDCAN_IT_RX_FIFO0_WATERMARK     /*!< Rx FIFO 0 fill level reached watermark */
#define BSP_FDCAN_IT_RX_FIFO0_NEW_MESSAGE  FDCAN_IT_RX_FIFO0_NEW_MESSAGE   /*!< New message written to Rx FIFO 0       */

/** @defgroup FDCAN_Rx_Fifo1_Interrupts FDCAN Rx FIFO 1 Interrupts
  * @{
  */
#define BSP_FDCAN_IT_RX_FIFO1_MESSAGE_LOST FDCAN_IT_RX_FIFO1_MESSAGE_LOST /*!< Rx FIFO 1 message lost                 */
#define BSP_FDCAN_IT_RX_FIFO1_FULL         FDCAN_IT_RX_FIFO1_FULL         /*!< Rx FIFO 1 full                         */
#define BSP_FDCAN_IT_RX_FIFO1_WATERMARK    FDCAN_IT_RX_FIFO1_WATERMARK    /*!< Rx FIFO 1 fill level reached watermark */
#define BSP_FDCAN_IT_RX_FIFO1_NEW_MESSAGE  FDCAN_IT_RX_FIFO1_NEW_MESSAGE  /*!< New message written to Rx FIFO 1       */

/** @defgroup FDCAN_Tx_Interrupts FDCAN Tx Interrupts
  * @{
  */
#define BSP_FDCAN_IT_TX_COMPLETE           FDCAN_IT_TX_COMPLETE   /*!< Transmission Completed                                */
#define BSP_FDCAN_IT_TX_ABORT_COMPLETE     FDCAN_IT_TX_ABORT_COMPLETE  /*!< Transmission Cancellation Finished                    */
#define BSP_FDCAN_IT_TX_FIFO_EMPTY         FDCAN_IT_TX_FIFO_EMPTY  /*!< Tx FIFO Empty                                         */

/** @defgroup FDCAN_flags FDCAN Flags
  * @{
  */
#define BSP_FDCAN_FLAG_TX_COMPLETE             FDCAN_FLAG_TX_COMPLETE             /*!< Transmission Completed                                */
#define BSP_FDCAN_FLAG_TX_ABORT_COMPLETE       FDCAN_FLAG_TX_ABORT_COMPLETE       /*!< Transmission Cancellation Finished                    */
#define BSP_FDCAN_FLAG_TX_FIFO_EMPTY           FDCAN_FLAG_TX_FIFO_EMPTY           /*!< Tx FIFO Empty                                         */
#define BSP_FDCAN_FLAG_RX_HIGH_PRIORITY_MSG    FDCAN_FLAG_RX_HIGH_PRIORITY_MSG    /*!< High priority message received                        */
#define BSP_FDCAN_FLAG_RX_BUFFER_NEW_MESSAGE   FDCAN_FLAG_RX_BUFFER_NEW_MESSAGE   /*!< At least one received message stored into a Rx Buffer */
#define BSP_FDCAN_FLAG_TX_EVT_FIFO_ELT_LOST    FDCAN_FLAG_TX_EVT_FIFO_ELT_LOST    /*!< Tx Event FIFO element lost                            */
#define BSP_FDCAN_FLAG_TX_EVT_FIFO_FULL        FDCAN_FLAG_TX_EVT_FIFO_FULL        /*!< Tx Event FIFO full                                    */
#define BSP_FDCAN_FLAG_TX_EVT_FIFO_WATERMARK   FDCAN_FLAG_TX_EVT_FIFO_WATERMARK   /*!< Tx Event FIFO fill level reached watermark            */
#define BSP_FDCAN_FLAG_TX_EVT_FIFO_NEW_DATA    FDCAN_FLAG_TX_EVT_FIFO_NEW_DATA    /*!< Tx Handler wrote Tx Event FIFO element                */
#define BSP_FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST   FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST   /*!< Rx FIFO 0 message lost                                */
#define BSP_FDCAN_FLAG_RX_FIFO0_FULL           FDCAN_FLAG_RX_FIFO0_FULL           /*!< Rx FIFO 0 full                                        */
#define BSP_FDCAN_FLAG_RX_FIFO0_WATERMARK      FDCAN_FLAG_RX_FIFO0_WATERMARK      /*!< Rx FIFO 0 fill level reached watermark                */
#define BSP_FDCAN_FLAG_RX_FIFO0_NEW_MESSAGE    FDCAN_FLAG_RX_FIFO0_NEW_MESSAGE    /*!< New message written to Rx FIFO 0                      */
#define BSP_FDCAN_FLAG_RX_FIFO1_MESSAGE_LOST   FDCAN_FLAG_RX_FIFO1_MESSAGE_LOST   /*!< Rx FIFO 1 message lost                                */
#define BSP_FDCAN_FLAG_RX_FIFO1_FULL           FDCAN_FLAG_RX_FIFO1_FULL           /*!< Rx FIFO 1 full                                        */
#define BSP_FDCAN_FLAG_RX_FIFO1_WATERMARK      FDCAN_FLAG_RX_FIFO1_WATERMARK      /*!< Rx FIFO 1 fill level reached watermark                */
#define BSP_FDCAN_FLAG_RX_FIFO1_NEW_MESSAGE    FDCAN_FLAG_RX_FIFO1_NEW_MESSAGE    /*!< New message written to Rx FIFO 1                      */
#define BSP_FDCAN_FLAG_RAM_ACCESS_FAILURE      FDCAN_FLAG_RAM_ACCESS_FAILURE      /*!< Message RAM access failure occurred                   */
#define BSP_FDCAN_FLAG_ERROR_LOGGING_OVERFLOW  FDCAN_FLAG_ERROR_LOGGING_OVERFLOW  /*!< Overflow of FDCAN Error Logging Counter occurred      */
#define BSP_FDCAN_FLAG_ERROR_PASSIVE           FDCAN_FLAG_ERROR_PASSIVE           /*!< Error_Passive status changed                          */
#define BSP_FDCAN_FLAG_ERROR_WARNING           FDCAN_FLAG_ERROR_WARNING           /*!< Error_Warning status changed                          */
#define BSP_FDCAN_FLAG_BUS_OFF                 FDCAN_FLAG_BUS_OFF                 /*!< Bus_Off status changed                                */
#define BSP_FDCAN_FLAG_RAM_WATCHDOG            FDCAN_FLAG_RAM_WATCHDOG            /*!< Message RAM Watchdog event due to missing READY       */
#define BSP_FDCAN_FLAG_ARB_PROTOCOL_ERROR      FDCAN_FLAG_ARB_PROTOCOL_ERROR      /*!< Protocol error in arbitration phase detected          */
#define BSP_FDCAN_FLAG_DATA_PROTOCOL_ERROR     FDCAN_FLAG_DATA_PROTOCOL_ERROR     /*!< Protocol error in data phase detected                 */
#define BSP_FDCAN_FLAG_RESERVED_ADDRESS_ACCESS FDCAN_FLAG_RESERVED_ADDRESS_ACCESS /*!< Access to reserved address occurred                   */
#define BSP_FDCAN_FLAG_TIMESTAMP_WRAPAROUND    FDCAN_FLAG_TIMESTAMP_WRAPAROUND    /*!< Timestamp counter wrapped around                      */
#define BSP_FDCAN_FLAG_TIMEOUT_OCCURRED        FDCAN_FLAG_TIMEOUT_OCCURRED        /*!< Timeout reached                                       */
#define BSP_FDCAN_FLAG_CALIB_STATE_CHANGED     FDCAN_FLAG_CALIB_STATE_CHANGED     /*!< Clock calibration state changed                       */
#define BSP_FDCAN_FLAG_CALIB_WATCHDOG_EVENT    FDCAN_FLAG_CALIB_WATCHDOG_EVENT    /*!< Clock calibration watchdog event occurred 			       */


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Function pointer type for FDCAN callback functions.
 */
typedef void (*BSP_FDCANRxFifoxCBPtr_t)(void* params);

/**
 * @brief Enumeration for BSP FDCAN operations.
 *
 * Represents the various fdcan-related operations available
 * within the BSP layer, abstracting HAL functions.
 */
typedef enum _BSP_FDCANOP_t {
    /* FDCAN Configuration Functions */
    BSP_FDCAN_CONFIGCLOCKCALIBRATION,
    BSP_FDCAN_GETCLOCKCALIBRATIONSTATE,
    BSP_FDCAN_RESETCLOCKCALIBRATIONSTATE,
    BSP_FDCAN_GETCLOCKCALIBRATIONCOUNTER,
    BSP_FDCAN_CONFIGFILTER,
    BSP_FDCAN_CONFIGGLOBALFILTER,
    BSP_FDCAN_CONFIGEXTENDEDIDMASK,
    BSP_FDCAN_CONFIGRXFIFOOVERWRITE,
    BSP_FDCAN_CONFIGFIFOWATERMARK,
    BSP_FDCAN_CONFIGRAMWATCHDOG,
    BSP_FDCAN_CONFIGTIMESTAMPCOUNTER,
    BSP_FDCAN_ENABLETIMESTAMPCOUNTER,
    BSP_FDCAN_DISABLETIMESTAMPCOUNTER,
    BSP_FDCAN_GETTIMESTAMPCOUNTER,
    BSP_FDCAN_RESETTIMESTAMPCOUNTER,
    BSP_FDCAN_CONFIGTIMEOUTCOUNTER,
    BSP_FDCAN_ENABLETIMEOUTCOUNTER,
    BSP_FDCAN_DISABLETIMEOUTCOUNTER,
    BSP_FDCAN_GETTIMEOUTCOUNTER,
    BSP_FDCAN_RESETTIMEOUTCOUNTER,
    BSP_FDCAN_CONFIGTXDELAYCOMPENSATION,
    BSP_FDCAN_ENABLETXDELAYCOMPENSATION,
    BSP_FDCAN_DISABLETXDELAYCOMPENSATION,
    BSP_FDCAN_ENABLEISOMODE,
    BSP_FDCAN_DISABLEISOMODE,
    BSP_FDCAN_ENABLEEDGEFILTERING,
    BSP_FDCAN_DISABLEEDGEFILTERING,

    /* FDCAN Control Functions */
    BSP_FDCAN_START,
    BSP_FDCAN_STOP,
    BSP_FDCAN_ADDMESSAGETOTXFIFOQ,
    BSP_FDCAN_ADDMESSAGETOTXBUFFER,
    BSP_FDCAN_ENABLETXBUFFERREQUEST,
    BSP_FDCAN_GETLATESTTXFIFOQREQUESTBUFFER,
    BSP_FDCAN_ABORTTXREQUEST,
    BSP_FDCAN_GETRXMESSAGE,
    BSP_FDCAN_GETTXEVENT,
    BSP_FDCAN_GETHIGHPRIORITYMESSAGESTATUS,
    BSP_FDCAN_GETPROTOCOLSTATUS,
    BSP_FDCAN_GETERRORCOUNTERS,
    BSP_FDCAN_ISRXBUFFERMESSAGEAVAILABLE,
    BSP_FDCAN_ISTXBUFFERMESSAGEPENDING,
    BSP_FDCAN_GETRXFIFOFILLLEVEL,
    BSP_FDCAN_GETTXFIFOFREELEVEL,
    BSP_FDCAN_ISRESTRICTEDOPERATIONMODE,
    BSP_FDCAN_EXITRESTRICTEDOPERATIONMODE,

    /* FDCAN TT Configuration and Control Functions */
    BSP_FDCAN_TT_CONFIGOPERATION,
    BSP_FDCAN_TT_CONFIGREFERENCEMESSAGE,
    BSP_FDCAN_TT_CONFIGTRIGGER,
    BSP_FDCAN_TT_SETGLOBALTIME,
    BSP_FDCAN_TT_SETCLOCKSYNCHRONIZATION,
    BSP_FDCAN_TT_CONFIGSTOPWATCH,
    BSP_FDCAN_TT_CONFIGREGISTERTIMEMARK,
    BSP_FDCAN_TT_ENABLEREGISTERTIMEMARKPULSE,
    BSP_FDCAN_TT_DISABLEREGISTERTIMEMARKPULSE,
    BSP_FDCAN_TT_ENABLETRIGGERTIMEMARKPULSE,
    BSP_FDCAN_TT_DISABLETRIGGERTIMEMARKPULSE,
    BSP_FDCAN_TT_ENABLEHARDWAREGAPCONTROL,
    BSP_FDCAN_TT_DISABLEHARDWAREGAPCONTROL,
    BSP_FDCAN_TT_ENABLETIMEMARKGAPCONTROL,
    BSP_FDCAN_TT_DISABLETIMEMARKGAPCONTROL,
    BSP_FDCAN_TT_SETNEXTISGAP,
    BSP_FDCAN_TT_SETENDOFGAP,
    BSP_FDCAN_TT_CONFIGEXTERNALSYNCPHASE,
    BSP_FDCAN_TT_ENABLEEXTERNALSYNCHRONIZATION,
    BSP_FDCAN_TT_DISABLEEXTERNALSYNCHRONIZATION,
    BSP_FDCAN_TT_GETOPERATIONSTATUS,

    /* FDCAN Interrupt Management */
    BSP_FDCAN_CONFIGINTERRUPTLINES,
    BSP_FDCAN_TT_CONFIGINTERRUPTLINES,
    BSP_FDCAN_ACTIVATENOTIFICATION,
    BSP_FDCAN_DEACTIVATENOTIFICATION,
    BSP_FDCAN_TT_ACTIVATENOTIFICATION,
    BSP_FDCAN_TT_DEACTIVATENOTIFICATION,
} BSP_FDCANOP_t;

/**
 * @brief Enumeration for BSP FDCAN identifiers.
 * Starts from 1 to align with common STM32 naming (FDCAN1, FDCAN2, ...)
 */
typedef enum _BSPFDCAN_t {
    BSP_FDCAN1 = 1,  ///< FDCAN 1 Identifier
    BSP_FDCAN2,      ///< FDCAN 2 Identifier
    BSP_FDCAN_COUNT
} BSP_FDCAN_t;

/**
 * @enum BSP_FDCANCBType_t
 * @brief FDCAN callback types for various fdcan events.
 */
typedef enum _BSP_FDCANCBType_t {
    BSP_FDCAN_CLOCKCALIBRATIONCALLBACK,
    BSP_FDCAN_TXEVENTFIFOCALLBACK,
    BSP_FDCAN_RXFIFO0CALLBACK,
    BSP_FDCAN_RXFIFO1CALLBACK,
    BSP_FDCAN_TXFIFOEMPTYCALLBACK,
    BSP_FDCAN_TXBUFFERCOMPLETECALLBACK,
    BSP_FDCAN_TXBUFFERABORTCALLBACK,
    BSP_FDCAN_RXBUFFERNEWMESSAGECALLBACK,
    BSP_FDCAN_TIMESTAMPWRAPAROUNDCALLBACK,
    BSP_FDCAN_TIMEOUTOCCURREDCALLBACK,
    BSP_FDCAN_HIGHPRIORITYMESSAGECALLBACK,
    BSP_FDCAN_ERRORCALLBACK,
    BSP_FDCAN_ERRORSTATUSCALLBACK,
    BSP_FDCAN_TT_SCHEDULESYNCCALLBACK,
    BSP_FDCAN_TT_TIMEMARKCALLBACK,
    BSP_FDCAN_TT_STOPWATCHCALLBACK,
    BSP_FDCAN_TT_GLOBALTIMECALLBACK,
    BSP_FDCAN_CALLBACK_TYPE_COUNT,
} BSP_FDCANCBType_t;

/**
 * @struct BSP_FDCANMap_t
 * @brief Maps BSP FDCAN enumerations to their corresponding HAL FDCAN handles.
 */
typedef struct _BSP_FDCANMap_t {
    BSP_FDCAN_t fdcan;       ///< Enumeration of the FDCAN
    FDCAN_HandleTypeDef *handle;  ///< Pointer to the HAL FDCAN handle
} BSP_FDCANMap_t;

#ifdef _USE_SEMAPHORE
typedef struct _BSP_FDCANSemMap_t {
	BSP_FDCAN_t fdcan;
	osSemaphoreId_t* BinSemHdlr;
	uint32_t timeout;
} BSP_FDCANSemMap_t;
#endif /* _USE_SEMAPHORE */

/**
 * @struct BSP_FDCANCB_t
 * @brief Manager BSP FDCAN Custom Callbacks and Parameters.
 */
typedef struct _BSP_FDCANCB_t {
    BSP_FDCANRxFifoxCBPtr_t callbacks[BSP_FDCAN_CALLBACK_TYPE_COUNT];
    void* params[BSP_FDCAN_CALLBACK_TYPE_COUNT];
} BSP_FDCANCB_t;

/**
  * @brief  FDCAN filter structure definition
  */
typedef struct _BSP_FDCANFilterTypeDef_t{
  uint32_t IdType;           /*!< Specifies the identifier type.
                                  This parameter can be a value of @ref FDCAN_id_type       */
  uint32_t FilterIndex;      /*!< Specifies the filter which will be initialized.
                                  This parameter must be a number between:
                                   - 0 and 127, if IdType is FDCAN_STANDARD_ID
                                   - 0 and 63, if IdType is FDCAN_EXTENDED_ID               */
  uint32_t FilterType;       /*!< Specifies the filter type.
                                  This parameter can be a value of @ref FDCAN_filter_type.
                                  The value FDCAN_EXT_FILTER_RANGE_NO_EIDM is permitted
                                  only when IdType is FDCAN_EXTENDED_ID.
                                  This parameter is ignored if FilterConfig is set to
                                  FDCAN_FILTER_TO_RXBUFFER                                  */
  uint32_t FilterConfig;     /*!< Specifies the filter configuration.
                                  This parameter can be a value of @ref FDCAN_filter_config */
  uint32_t FilterID1;        /*!< Specifies the filter identification 1.
                                  This parameter must be a number between:
                                   - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                                   - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID       */
  uint32_t FilterID2;        /*!< Specifies the filter identification 2.
                                  This parameter is ignored if FilterConfig is set to
                                  FDCAN_FILTER_TO_RXBUFFER.
                                  This parameter must be a number between:
                                   - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                                   - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID       */
  uint32_t RxBufferIndex;    /*!< Contains the index of the Rx buffer in which the
                                  matching message will be stored.
                                  This parameter must be a number between 0 and 63.
                                  This parameter is ignored if FilterConfig is different
                                  from FDCAN_FILTER_TO_RXBUFFER                             */
  uint32_t IsCalibrationMsg; /*!< Specifies whether the filter is configured for
                                  calibration messages.
                                  This parameter is ignored if FilterConfig is different
                                  from FDCAN_FILTER_TO_RXBUFFER.
                                  This parameter can be:
                                   - 0 : ordinary message
                                   - 1 : calibration message                                */
} BSP_FDCANFilterTypeDef_t;

/**
  * @brief  FDCAN Tx header structure definition
  */
typedef struct _BSP_FDCANTxHeaderTypeDef_t{
  uint32_t Identifier;          /*!< Specifies the identifier.
                                     This parameter must be a number between:
                                      - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                                      - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID               */
  uint32_t IdType;              /*!< Specifies the identifier type for the message that will be
                                     transmitted.
                                     This parameter can be a value of @ref FDCAN_id_type               */
  uint32_t TxFrameType;         /*!< Specifies the frame type of the message that will be transmitted.
                                     This parameter can be a value of @ref FDCAN_frame_type            */
  uint32_t DataLength;          /*!< Specifies the length of the frame that will be transmitted.
                                      This parameter can be a value of @ref FDCAN_data_length_code     */
  uint32_t ErrorStateIndicator; /*!< Specifies the error state indicator.
                                     This parameter can be a value of @ref FDCAN_error_state_indicator */
  uint32_t BitRateSwitch;       /*!< Specifies whether the Tx frame will be transmitted with or without
                                     bit rate switching.
                                     This parameter can be a value of @ref FDCAN_bit_rate_switching    */
  uint32_t FDFormat;            /*!< Specifies whether the Tx frame will be transmitted in classic or
                                     FD format.
                                     This parameter can be a value of @ref FDCAN_format                */
  uint32_t TxEventFifoControl;  /*!< Specifies the event FIFO control.
                                     This parameter can be a value of @ref FDCAN_EFC                   */
  uint32_t MessageMarker;       /*!< Specifies the message marker to be copied into Tx Event FIFO
                                     element for identification of Tx message status.
                                     This parameter must be a number between 0 and 0xFF                */
} BSP_FDCANTxHeaderTypeDef_t;

/**
  * @brief  FDCAN Rx header structure definition
  */
typedef struct _BSP_FDCANRxHeaderTypeDef_t{
  uint32_t Identifier;            /*!< Specifies the identifier.
                                       This parameter must be a number between:
                                        - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                                        - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID               */
  uint32_t IdType;                /*!< Specifies the identifier type of the received message.
                                       This parameter can be a value of @ref FDCAN_id_type               */
  uint32_t RxFrameType;           /*!< Specifies the the received message frame type.
                                       This parameter can be a value of @ref FDCAN_frame_type            */
  uint32_t DataLength;            /*!< Specifies the received frame length.
                                        This parameter can be a value of @ref FDCAN_data_length_code     */
  uint32_t ErrorStateIndicator;   /*!< Specifies the error state indicator.
                                       This parameter can be a value of @ref FDCAN_error_state_indicator */
  uint32_t BitRateSwitch;         /*!< Specifies whether the Rx frame is received with or without bit
                                       rate switching.
                                       This parameter can be a value of @ref FDCAN_bit_rate_switching    */
  uint32_t FDFormat;              /*!< Specifies whether the Rx frame is received in classic or FD
                                       format.
                                       This parameter can be a value of @ref FDCAN_format                */
  uint32_t RxTimestamp;           /*!< Specifies the timestamp counter value captured on start of frame
                                       reception.
                                       This parameter must be a number between 0 and 0xFFFF              */
  uint32_t FilterIndex;           /*!< Specifies the index of matching Rx acceptance filter element.
                                       This parameter must be a number between:
                                        - 0 and 127, if IdType is FDCAN_STANDARD_ID
                                        - 0 and 63, if IdType is FDCAN_EXTENDED_ID
                                       When the frame is a Non-Filter matching frame, this parameter
                                       is unused.                                                        */
  uint32_t IsFilterMatchingFrame; /*!< Specifies whether the accepted frame did not match any Rx filter.
                                       Acceptance of non-matching frames may be enabled via
                                       HAL_FDCAN_ConfigGlobalFilter().
                                       This parameter takes 0 if the frame matched an Rx filter or
                                       1 if it did not match any Rx filter                               */
} BSP_FDCANRxHeaderTypeDef_t;


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */




/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
BSP_StatusTypeDef_t BSP_InitFDCAN(BSP_FDCAN_t fdcan);
BSP_StatusTypeDef_t BSP_DeInitFDCAN(BSP_FDCAN_t fdcan);

/* ------------------- FDCAN CHECK ------------------- */
BSP_StatusTypeDef_t BSP_GetStateFDCAN(BSP_FDCAN_t fdcan);
BSP_StatusTypeDef_t BSP_GetErrorFDCAN(BSP_FDCAN_t fdcan);

/* ------------------- CONFIGURATION FUNCTIONS ------------------- */
BSP_StatusTypeDef_t BSP_ConfigFDCANFilter(BSP_FDCAN_t fdcan, BSP_FDCANFilterTypeDef_t* sFilterConfig);
BSP_StatusTypeDef_t BSP_ConfigFDCANGlobalFilter(BSP_FDCAN_t fdcan, uint32_t nonMatchingStd, uint32_t NonMatchingExt, uint32_t RejectRemoteStd, uint32_t RejectRemoteExt);
BSP_StatusTypeDef_t BSP_ConfigFDCANTxDelay(BSP_FDCAN_t fdcan, uint32_t tdcFilter);
BSP_StatusTypeDef_t BSP_EnableFDCANTxDelay(BSP_FDCAN_t fdcan);

/* ------------------- INTERRUPTS MANAGEMENT ------------------- */
BSP_StatusTypeDef_t BSP_ActivateFDCANNotification(BSP_FDCAN_t fdcan, uint32_t activeIT, uint32_t buffIndex);

/* ------------------- CONTROL FUNCTIONS ------------------- */
BSP_StatusTypeDef_t BSP_StartFDCAN(BSP_FDCAN_t fdcan);
BSP_StatusTypeDef_t BSP_StopFDCAN(BSP_FDCAN_t fdcan);
BSP_StatusTypeDef_t BSP_AddFDCANMsgToTxQ(BSP_FDCAN_t fdcan, BSP_FDCANTxHeaderTypeDef_t* pTxHeader, uint8_t* pTxData);
BSP_StatusTypeDef_t BSP_GetFDCANRxMsg(BSP_FDCAN_t fdcan, uint32_t RxLocation, BSP_FDCANRxHeaderTypeDef_t* pRxHeader, uint8_t* pRxData);

/* ------------------- FDCAN CALLBACKS ------------------- */
void BSP_SetFDCANCB(BSP_FDCAN_t fdcan, BSP_FDCANCBType_t callbackType, BSP_FDCANRxFifoxCBPtr_t callback, void* params);


#endif /* HAL_FDCAN_MODULE_ENABLED */

#endif /* FDCAN_INC_BSP_FDCAN_H_ */
