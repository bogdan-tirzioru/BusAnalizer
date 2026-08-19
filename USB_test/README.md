# BusAnalyzer 1 USB test project

This STM32CubeMX project targets the original BusAnalyzer 1 board:

- MCU: STM32H750VBTx, LQFP100
- board supply: LDO
- HSE: 8 MHz crystal on PH0/PH1
- USB peripheral: USB OTG HS controller with embedded Full-Speed PHY
- USB pins: PB14/USB_DM and PB15/USB_DP
- middleware: USB Device CDC ACM
- logging: USART1, PA9/TX and PA10/RX, 115200-8-N-1
- debugging: SWD on PA13/PA14

CAN, RTC, SPI, I2C and timers are intentionally not configured.

## Generate with STM32CubeMX / STM32CubeIDE

1. Open `USB_test.ioc`.
2. Allow CubeMX to migrate the old known-working BusAnalyzer configuration to the installed STM32CubeH7 package.
3. Confirm the settings below before generating code.
4. Select STM32CubeIDE as the toolchain and choose **Generate Code**.

Required checks:

- RCC HSE is **Crystal/Ceramic Resonator**, 8 MHz.
- SYSCLK is 480 MHz and the USB kernel clock is exactly 48 MHz.
- USB OTG HS is **Device Only**, using the **embedded Full-Speed PHY**.
- USB DMA is disabled.
- VBUS sensing is disabled.
- USB Device middleware class is CDC.
- PB14 is USB_OTG_HS_DM and PB15 is USB_OTG_HS_DP.
- USART1 is asynchronous at 115200 baud, with PA9/TX and PA10/RX.
- Debug is Serial Wire, not full JTAG.

## Critical power check

Before flashing, inspect the generated `SystemClock_Config()` and verify that it contains:

```c
HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
```

Do not flash firmware generated with `PWR_DIRECT_SMPS_SUPPLY` on this board.

The USB controller is named `USB_OTG_HS` by STM32Cube, but this board uses its internal Full-Speed PHY. Linux should therefore report a 12 Mbit/s USB connection.
