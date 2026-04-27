본 폴더에 있는 소스코드들은

STM32CubeIDE의 .ioc 편집을 하는 과정에

서로 호환되지 아니하거나 기타 사유로 인해 생성에 충돌이 일어나는 경우,

해당 폴더에 보관하여 컴파일에 참조할 수 있도록 관리하는 폴더입니다.

********************************************************************************
자동 생성된 파일 이외에는 본 폴더에 파일을 추가하는 것을 엄금합니다
********************************************************************************

자동 생성된 파일 이외에 기존 코드에서 추가된 파일은

- Compatible/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_compatible.h 

1 종이며, 해당 헤더는

- Core/Inc/stm32h7xx_hal_conf.h

폴더에서 USER Include 영역에 추가해야 올바르게 빌드가 가능합니다.

--------------------------------------------------------------------------------
[USER CODE 블록 커스터마이징 내역]
CubeMX 재생성 시 USER CODE 블록 내부는 보존됩니다.
아래 파일들에 USER CODE 블록 내 커스텀 코드가 포함되어 있습니다:

1. USB_DEVICE/App/usbd_cdc_if.c (.h 포함)
   - Non-Cacheable DMA 버퍼 (D3 RAM 배치)
   - IOIF 콜백 등록 (Tx완료, Rx수신, DTR상태변경)
   - CDC_SET_CONTROL_LINE_STATE에서 DTR 감지 (Host 연결/해제 이벤트)
   
2. USB_DEVICE/App/usb_device.c
   - HAL_PWREx_EnableUSBVoltageDetector() 호출
   
3. USB_DEVICE/App/usbd_desc.c
   - stm32h7xx_compatible.h include 및 조건부 컴파일 가드
   
4. USB_DEVICE/Target/usbd_conf.c
   - USB FIFO 크기 설정
--------------------------------------------------------------------------------

자세한 사항은 펌웨어팀에 연락 하세용



펌웨어팀 선임연구원 김진우