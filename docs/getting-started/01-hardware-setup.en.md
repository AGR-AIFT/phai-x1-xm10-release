# 01 — Hardware Setup

Connect the XM10 board, the KIT H10 robot, the ST-Link debugger, and the USB cable in the right order. The whole process takes about 15 minutes.

The XM10 draws 24 V power and communicates through the KIT H10 chassis. A single wrong connection can prevent the board from powering on, break robot communication, or block firmware uploads. In practice, wiring mistakes cost more time than writing the firmware itself. Take it slow the first time.

---

## What You Need

| Category | Item |
|----------|------|
| Robot | KIT H10 chassis |
| Board | XM10 board, Sensor Hub Module board |
| Cables | XM10 ↔ KIT H10 extension cable, XM10 ↔ Sensor Hub cable |
| Debugging | ST-Link V2 debugger + 20-to-4 pin adapter board + 4-pin SWD cable |
| Accessories | USB-C cable (for data monitoring), SanDisk Ultra Dual Drive Type-C 32 GB (for MSC logging) |
| PC | PC with STM32CubeIDE installed, (optional) Jetson Orin NX or similar AM |

### Checking the KIT H10 Firmware Version

The XM10 and KIT H10 communicate with each other, so both sides must be running compatible firmware versions.

| XM FW | Required KIT H10 FW |
|:---:|:---:|
| v2.2.2 (latest) | CM v2.3.0 / ESP32 v2.3.0 / SAM10 v2.3.0 |
| v2.0.x | CM v2.3.0 / ESP32 v2.3.0 / SAM10 v2.3.0 |
| v1.0.x | Factory-shipped version (no update needed) |

If the versions don't match → update using the [KIT H10 Firmware Guide](../kit-h10-firmware/).

---

## Connection Steps

### 1. KIT H10 ↔ XM10

Locate the **extension cable** tucked inside the left actuator apparel of the KIT H10, then plug it into the main connector on the XM10. This single cable carries both 24 V power and CAN-FD communication.

<figure markdown="span">
  ![KIT H10 ↔ XM10 connection](https://github.com/user-attachments/assets/cac2643d-532b-41a6-a680-7fb57d69d2af){ width="90%" }
  <figcaption>Figure 1. KIT H10 ↔ XM10 connection</figcaption>
</figure>

**Connector pinmap (Molex 1053081206):**

| Pin | Function |
|-----|----------|
| 1 | NC |
| 2 | 24 V |
| 3 | GND |
| 4 | GND |
| 5 | CAN HIGH |
| 6 | CAN LOW |

✅ Check: Make sure the cable is fully seated (a partially inserted connector will silently drop communication only).

### 2. ST-Link Debugger (PC ↔ XM10)

Required for firmware uploads and live debugging. You only need to flash via SWD once; after that you can also upload through PhAI Studio's USB FTP ([bootloader guide](../bootloader/)).

1. Connect the ST-Link debugger to the PC via USB.
2. Connect the ST-Link's SWD output to the XM10's 4-pin SWD port using the adapter board and SWD cable.

<figure markdown="span">
  ![ST-Link SWD pinmap](https://github.com/user-attachments/assets/a0fccf85-d6af-4efe-b6ab-615710f34cec){ width="60%" }
  <figcaption>Figure 2. ST-Link SWD pinmap</figcaption>
</figure>

✅ Check: Confirm that `STMicroelectronics STLink` appears in Windows Device Manager.

### 3. Sensor Hub (Optional)

To use sensors such as EMG, ground reaction force (GRF), or FSR, connect the Sensor Hub board to the expansion port on the XM10. This is not required until Ex.07 or later.

### 4. USB-C Data Cable (Optional)

Used for PhAI Studio real-time monitoring and USB memory logging. Required for examples Ex.07 through Ex.10c.

---

## Common Issues

- **The board power LED does not light up** — Verify that the KIT H10 main power is ON → confirm the main connector is fully inserted → check the cable for breaks (measure 24 V with a multimeter).
- **The ST-Link is not recognized** — Try a different USB port. Prefer a rear-panel USB-A port on the PC and avoid USB hubs.
- **"Target no device found"** — ST-Link is recognized but the MCU is not responding. Check board power and verify the 4-pin SWD orientation (pin 1 marker).
- **KIT H10 works fine but XM10 is unresponsive** — The CAN HIGH/LOW lines may be swapped. Recheck the pinmap in Figure 1.
- **No LED after boot but ST-Link connects** — The bootloader may not be installed. See the [bootloader guide](../bootloader/).
- **The Sensor Hub is not working** — Check the firmware version on the hub board itself.

---

## Next Steps

Once all connections are in place and the board power LED is on → [02 Software Setup](02-software-setup.md)

To work through the setup together with AI → [Getting Started with Claude Code](00-claude-code-quickstart.md)
