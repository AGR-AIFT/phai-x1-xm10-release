# KIT H10 Firmware & Contents File Update

> 📌 **After reading this page**: You will be able to update the KIT H10 (CM / ESP32 / SAM10) firmware and ContentsFiles — matched to your XM firmware version — using either a USB stick or an SD card.
> ⏱️ Estimated reading time: 20 min (hands-on: 30–60 min)
> 🧰 Prerequisites: KIT H10 hardware architecture ([docs/architecture/README.md](../architecture/README.md))
> 🎯 Key point: Compatibility matrix — **XM v2.6.0 ↔ KIT H10 v2.4.0 ↔ ContentsFiles (attached to the Release)** (mixing versions is not allowed)

> ⚠️ **Do not mix versions** — combining the latest XM FW with H10 v1.0.x (or vice versa) causes CAN-FD protocol mismatch and communication errors. Apply the matrix exactly as shown.

XM10 communicates with KIT H10 over CAN-FD, so you must use **the KIT H10 firmware and ContentsFiles that match your XM firmware version**. A version mismatch will cause communication errors or unexpected behavior.

> **Always apply the KIT H10 firmware and ContentsFiles that correspond to your XM firmware version.**

---

## 🎯 Quick Decision Guide

| Situation | What you need | Where to go |
|-----------|--------------|-------------|
| **New XM10 received, H10 is also new** | Check the compatibility matrix — if they match, proceed as-is | ↓ Matrix |
| **XM10 is up to date but H10 is an older version** | Update CM / SAM10 / ESP32 + ContentsFiles | ↓ Firmware Update (USB stick) |
| **Firmware is OK, only motion map / audio needs updating** | Replace ContentsFiles only (SD card swap) | ↓ Contents File Update (SD card) |
| **Red LED during update** | An error occurred | ↓ Error reference table |
| **Manual update is not feasible** | Contact support | [Discussions Q&A](https://github.com/AGR-AIFT/phai-x1-xm10-release/discussions/categories/q-a) |

---

## Version Compatibility Matrix

| XM FW Version | KIT H10 FW Version | ContentsFiles | Notes |
| :---: | :---: | :---: | :--- |
| **v2.6.0** | **CM v2.4.0 / SAM10 v2.4.0 / ESP32 v2.3.0** | **Attached to the Release** | **Latest — recommended** |
| v2.3.0 – v2.5.1 | CM v2.3.0 / SAM10 v2.3.0 / ESP32 v2.3.0 | Attached to the Release | Previous |
| v2.0.x – v2.2.2 | CM v2.3.0 / SAM10 v2.3.0 / ESP32 v2.3.0 | 2025.02 – 2026.04 | Previous |
| v1.0.x | Factory-shipped version | Factory-shipped version | Legacy |

> **v1.0.x users:** If you continue using XM FW v1.0.x without upgrading, you must keep the KIT H10 firmware at its factory-shipped version as well. Mixing the latest XM FW with an older H10 firmware causes a protocol mismatch and will prevent normal operation.
> Note that the KIT H10 firmware paired with v1.0.x has known issues (such as motor communication errors) that have not been resolved, so upgrading both XM and KIT H10 to the latest firmware is strongly recommended.

---

## Downloads

All files are available on the GitHub Releases page.

**[Go to Releases](https://github.com/AGR-AIFT/phai-x1-xm10-release/releases)**

### Files attached to v2.6.0

> KIT H10 firmware moves to **CM · SAM10 v2.4.0** in this release (ESP32 stays at v2.3.0). The contents files are unchanged; only the filename now carries a date.

| Filename | Purpose | Size |
| :--- | :--- | :---: |
| `SUIT_H10_Binary_YYYYMMDD.zip` | CM · SAM10 · ESP32 firmware bundle | Unzip to get the three `.bin` files |
| `SUIT_ContentsFiles_YYYYMMDD.zip` | SD card contents files (audio, FSM, motion map, etc.) | Unzip onto the SD card |

---

## Firmware Update (USB Stick Method)

Use a USB stick to update the CM and SAM10 (MD) firmware on the KIT H10.

### KIT H10 Firmware Storage Structure

Each module (CM, SAM10) has two regions in internal Flash: a **bootloader** and an **application firmware**.

| Region | Role |
| :--- | :--- |
| **Bootloader** | Executes first at power-on. Performs an update if one is pending; otherwise launches the application firmware |
| **Application FW** | The main firmware that implements all robot functionality |

### What You Need

1. **A USB stick with a USB-C connector** (or a USB-A stick with a USB-C adapter)
2. USB stick formatted as **FAT32** (at least 10 MB free space)
3. Firmware binary files copied to the **root directory** of the USB stick

> **Filename rules (must be followed exactly)**
> - CM: `SUIT_CM_APP_{major}_{minor}_{patch}.bin`
> - MD (SAM10): `SUIT_SAM10_APP_{major}_{minor}_{patch}.bin`
> - Each version number component must be in the range 0–255

### Update Procedure

#### Scenario 1: Update CM only

Place only the CM file on the USB stick — only the CM will be updated.

#### Scenario 2: Update both CM and SAM10 (MD)

Place both the CM and SAM10 files on the USB stick — they will be updated sequentially.

```
Update CM → Done → Update left SAM10 → Done → Update right SAM10 → All done
```

#### Step-by-Step

| Step | Action | LED Status |
| :---: | :--- | :--- |
| 1 | Insert the USB stick into the **USB-C port** at the bottom of the CM module | — |
| 2 | Hold the `+` button (assist increase) and power on | — |
| 3 | Bootloader starts (waits 1.5 s for SAM10 recovery signal) | All white LEDs ON |
| 4 | Beep → release the button. Bootloader identifies the files to update | Light-green LEDs ON (one per target module) |
| 5 | CM update in progress → complete | White LEDs show progress → one light-green LED turns OFF |
| 6 | (CM + MD mode) MD file download → CAN-FD communication ready | Green LED ON |
| 7 | Left SAM10 update in progress | White LEDs show progress |
| 8 | Left update complete → right SAM10 update in progress | Light-green LED OFF → right side progress |
| 9 | All updates complete → power off | **Blue LED** + beep every 3 seconds |

### LED Status Summary

| LED Color | Meaning |
| :--- | :--- |
| White | Update in progress (0–100%, in 10% increments) |
| Light-green | Indicates target modules (3: CM + SAM10 L/R; 1: CM only; 2: SAM10 L/R only) |
| Green | CM ↔ SAM10 CAN-FD communication ready |
| **Blue** | **All updates completed successfully** |
| **Red** | **Error occurred** (beep every 2 seconds) |

### Error Reference

| Situation | Symptom | Resolution |
| :--- | :--- | :--- |
| USB stick not inserted | White ON → beep → OFF → normal boot | Insert USB stick and retry |
| File missing / wrong filename | White ON → beep → OFF → red after 5–6 s | Check filename rules and retry |
| CM / SAM10 version number invalid | Light-green × 3 ON → red | Verify version numbers in the binary filename are in range 0–255 |
| SAM10 version number invalid only | CM update completes → red | Correct the SAM10 filename and retry |

> The steps above are all you need. If you get stuck, ask on [Discussions Q&A](https://github.com/AGR-AIFT/phai-x1-xm10-release/discussions/categories/q-a).

---

## Contents File Update (SD Card Method)

Update the contents files stored on the SD card inside the KIT H10 CM module.

### Contents File Structure

The CM module contains an internal SD card (SanDisk Micro SD Max Endurance) with the following folders:

| Folder | Contents |
| :--- | :--- |
| `AudioFiles/` | H10 audio files |
| `ContentsFiles/` | FSM control data files, FSM-based motion maps, robot basic parameters |
| `LOG/` | H10 error logs |
| `RobotData/` | H10 internal data storage |

### Update Procedure

| Step | Action | Notes |
| :---: | :--- | :--- |
| 1 | **Power off KIT H10** and disconnect the battery | Always cut power for safety |
| 2 | Detach the CM module from the apparel | Release the apparel fasteners in order |
| 3 | Detach the CM module from the frame | Remove the screws |
| 4 | Remove the SD card cover, then **eject the SD card** | Use tweezers or a pointed tool to remove the cover |
| 5 | Connect the SD card to your PC using a card reader | — |
| 6 | **Delete** the existing contents files, then copy the new files | Extract `SUIT_ContentsFiles_*.zip` and copy the contents |
| 7 | Reinsert the SD card → reassemble the CM module | — |
| 8 | Power on and verify normal operation | The firmware must be up to date for correct operation |

> **Caution:** When replacing contents files, always **delete all existing files first** before copying the new files.

> The steps above are all you need. If you get stuck, ask on [Discussions Q&A](https://github.com/AGR-AIFT/phai-x1-xm10-release/discussions/categories/q-a).

### If Manual Steps Are Not Feasible

| Channel | Purpose |
|---------|---------|
| [GitHub Discussions Q&A](https://github.com/AGR-AIFT/phai-x1-xm10-release/discussions/categories/q-a) | General questions and procedure inquiries |
| [GitHub Issues](https://github.com/AGR-AIFT/phai-x1-xm10-release/issues) | Bug reports and feature requests |

---

## Frequently Asked Questions

**Q: I installed XM v2.3.0 but haven't updated the KIT H10 firmware. What happens?**
> The CAN-FD protocol has changed, so communication errors will occur. Update to the corresponding H10 firmware immediately.

**Q: How do I update the ESP32 firmware?**
> The ESP32 firmware (`SUIT_ESP32_FW_X_X_X.bin`) is transmitted from the CM to the ESP32 automatically during CM boot. Simply include it on the USB stick alongside the CM firmware — it will be handled automatically during the CM update process.

**Q: Can I keep using XM FW v1.0.x?**
> Yes, but you must also keep the KIT H10 firmware at its factory-shipped version. Do not mix XM FW v1.0.x with the latest H10 FW.

**Q: What should I do if an error occurs (red LED) during the update?**
> Power off the device, verify the filenames and format (FAT32) on the USB stick, then try again. If the error persists, contact support using the channels listed above.
