# 🎬 Getting Started with PhAI-X1 — Tutorial Shooting Direction (v2, English)

> **Who this video is for**: a student who just received PhAI-X1 (the H10 exoskeleton + the XM10 board) for the first time.
> **What the viewer can do by the end**: ① put their own code on the board and **make the robot move**, and ② see that data live in **PhAI Studio**. (A later video adds EMG-based control.)
> **Filming baseline**: **Rev 2.0 board only**, firmware = **latest XM-Dev**. Flashing via ST-Link + STM32CubeIDE.
> **Tone**: friendly. Explain each piece of jargon once in plain words, and reassure ("it's OK if you get stuck here").

---

## 0. The Big Picture (summary for the videographer)

This video shows the whole path "from nothing → the robot moves → the data appears," split into three small wins (Hello Worlds).

| Step | The win to show | One line |
|---|---|---|
| **Win 1 — Run** | The robot legs swing ±25° by themselves | Load the example, press the suit button, the robot moves |
| **Win 2 — Data** | That motion appears as a live PhAI Studio graph | Connect over USB and the board's data shows up live |
| **(next video) EMG** | Control from muscle signals | A separate video. This one lays the groundwork |

**Runtime ≈ 14–16 min.** Chapter timeline:
```
00:00  Intro — what is PhAI-X1?
00:45  ① Connect the hardware
03:00  ② Set up the dev environment (install + get the SDK)
06:00  ③ Bring in the example code (Passive Mode)
08:30  ④ Build and flash to the board
10:30  ⑤ Wake the robot — press the suit button, it moves  ★climax
12:30  ⑥ See the data in PhAI Studio
14:30  ⑦ Safety note + next-video (EMG) teaser
```

> 💡 Filming tip: prepare both **screen capture** (CubeIDE/Studio) and a **real camera** (board, LEDs, robot). The edit cuts between them. In the tables, "Cam" column: [S]=screen capture, [R]=real camera.

---

## 1. Prepare Before Filming

### Hardware (all Rev 2.0)
- KIT H10 exoskeleton + XM10 Rev 2.0 board (already joined and cabled together)
- ST-Link V2 debugger + 20→4-pin adapter + 4-pin SWD cable (the tool that loads code onto the board)
- USB-C **data** cable (for monitoring — a charge-only cable will NOT work)
- The robot **mounted on a stand/jig**, area cleared, with **one person on stop duty**

### Software
- STM32CubeIDE v2.0.0+ (free, ST account required)
- XM10 SDK (latest XM-Dev → release ZIP)
- PhAI Studio (for data monitoring)

> ⚠️ **Friendly note**: in the video, reassure the student with lines like "the installs take a little while — grab them ahead of time."

---

## 2. Main Scene List

### ① Intro (~45 s)
| Scene | Cam | On screen | Narration |
|---|---|---|---|
| 1-1 | [R] | H10 and XM10 side by side | "PhAI-X1 is these two together — the robot (H10) and the dev board (XM10). The XM10 is powered by the H10, so they turn on together." |
| 1-2 | [S] | Preview: robot swinging + graph moving | "Follow along to the end and you'll build this yourself." |

### ② Connect the Hardware (~2 min 15 s)
> Friendly note: "Honestly, people get stuck on cables more than on code. Let's go slow."

| Scene | Cam | On screen | Narration / caption | Watch out |
|---|---|---|---|---|
| 2-1 | [R] | Push the H10 expansion cable **all the way** into the XM10 | "This one cable carries both power and communication. Push it in firmly." | If loose, only comms drop — confusing |
| 2-2 | [R] | ST-Link into a rear PC USB port → SWD 4-pin to the board (align pin 1) | "ST-Link is the tool that loads code onto the board. Plug it straight into the PC, not a USB hub." | Close-up on the pin-1 marker |
| 2-3 | [R] | Power on H10 → XM10 board LED lights up | "When the board's light comes on, it's alive. Then plug in USB-C." | If no LED, recheck the cable |

### ③ Set Up the Dev Environment (~3 min)
| Scene | Cam | On screen | Narration / caption | Watch out |
|---|---|---|---|---|
| 3-1 | [S] | Install STM32CubeIDE (ST login → download → install) | "This one program already includes the compiler and drivers. Install just this and you're set." | No Korean/spaces in the install path |
| 3-2 | [S] | Extract the SDK ZIP to `C:\dev` → check `.project`/`CLAUDE.md` exist | "This extracted folder is our project folder." | Don't use a OneDrive folder (path conflicts) |

> Since we film Rev 2.0 only, we skip the "which board revision?" step and go straight to the Rev 2.0 SDK.

### ④ Bring in the Example Code — Passive Mode (~2 min 30 s)
> Friendly note: "You don't write code from scratch — you reuse an example."

On the XM10, **the only place you write code is the `XM_Apps/Control_Task/` folder.** There are two ways to bring an example in — show both.

**Method 1 — Overwrite the contents (simplest, recommended)**
| Scene | Cam | On screen | Narration |
|---|---|---|---|
| 4-1a | [S] | Open `examples/11_Passive_Mode/passive_mode.c` → Select All (Ctrl+A) → Copy → open `Control_Task/control_task.c` and paste over everything | "Copy the whole example and paste it right into the existing control_task.c. You keep just one file." |

**Method 2 — Copy the whole file (one thing to watch!)**
| Scene | Cam | On screen | Narration |
|---|---|---|---|
| 4-1b | [S] | Copy `passive_mode.c` into the `Control_Task/` folder → now **both** `control_task.c` and `passive_mode.c` exist | "If you copy the file, you end up with two files. But both define `Control_Setup`/`Control_Loop`, so building as-is gives a 'function defined twice' error." |
| 4-1c | [S] | Show two fixes: (a) **delete** `control_task.c`, or (b) right-click `control_task.c` → Resource Configurations → **Exclude from Build** | "So only one of them should go into the build. Either **delete** the original control_task.c, or **exclude it from the build**. If deleting feels risky, exclude it — it's easy to undo later." |

> Caption card (10 s): **"Duplicate-function error? → Delete control_task.c, or exclude it from the build!"**

### ⑤ Build and Flash (~2 min)
| Scene | Cam | On screen | Narration / caption | Watch out |
|---|---|---|---|---|
| 5-1 | [S] | `Ctrl+B` (build) → `0 errors` in the Console | "Building translates your code into the board's language. The first build takes ~5 min. Just look for 'errors 0' — that's success!" | Speed up the 5-min build in editing |
| 5-2 | [S] | Click the bug icon (Debug) → it flashes automatically → Resume (F8) | "The bug icon makes ST-Link load the code. Press Resume to start running." | "No ST-Link" → re-plug USB |

### ⑥ Wake the Robot ★Climax (~2 min)
> Friendly note: "This is the fun part." And the **one key thing**: flashing alone does NOT make the robot move.

| Scene | Cam | On screen | Narration / caption | Watch out |
|---|---|---|---|---|
| 6-1 | [S] | Board auto-connects to CM (H10 central module) → STANDBY | "Once the board powers up, it auto-connects to the robot and waits in STANDBY." | — |
| 6-2 | [R] | **Press the ASSIST-mode button on the H10 suit** → robot homes to 0° → legs **swing smoothly ±25°** | "Here's the key thing! The robot wakes up only when you press the **ASSIST button on the suit** — that's a button on the **robot body**, not on the board. Press it — and watch, it starts moving!" | **ASSIST button is on the H10 body** |
| 6-3 | [R] | Robot swing, 2 angles (side + joint close-up), 5 s+ | (sfx) "That's your first Hello World — your code made the robot move." | — |

> 🎥 Climax direction: keep the 3-beat "press suit button → brief homing → swing starts" in one breath. Emphasize the very first motion.

### ⑦ See the Data in PhAI Studio — Win 2 (~2 min)
| Scene | Cam | On screen | Narration / caption | Watch out |
|---|---|---|---|---|
| 7-1 | [S+R] | Plug USB-C → **open PhAI Studio → select the port → Connect** → joint angles ripple in the graph exactly with the robot motion | "Even without extra code, the board streams data by default. Open PhAI Studio and just press **Connect** — it shows up immediately. Plugging the cable isn't enough; you have to **Connect** in the Studio." | **Connect is required** — cable alone won't start it |
| 7-2 | [R+S] | Robot swing and graph ripple side by side | "As the robot on the left moves, the graph on the right moves the same way — that's your second Hello World." | — |

> Detailed PhAI Studio usage (channel select, recording, CSV export) is covered in a **separate guide/video**. This video only goes as far as "Connect and the data appears."

### ⑧ Safety Note + Next-Video Teaser (~1 min 30 s)
| Scene | Cam | On screen | Narration |
|---|---|---|---|
| 8-1 | [R] | Robot on its stand + point at the suit STANDBY button | "This robot actually produces force. So always: **verify on the stand first**, the **suit STANDBY button is your emergency stop**, and keep **someone ready to stop it**. (Details are in the safety guide doc.)" |
| 8-2 | [S] | EMG teaser: a "muscle signal → assist" shot | "Next time, we'll read **your muscle signals** with an EMG sensor so the robot helps you. You read the signal (`XM_SetControlMode(TORQUE)` + `XM_SetAssistTorque`), and **you design which motion to assist and how**. Stay tuned!" |

---

## 3. Common Snags & Backup Plan

| Symptom | Cause | On-set fix |
|---|---|---|
| Board LED won't light | Cable not fully seated / H10 off | Reseat → check H10 power |
| "No ST-Link detected" | USB hub/cable | Plug straight into the rear PC port |
| Duplicate-function build error | Both control_task.c and the example .c exist | Delete control_task.c or exclude from build (see ④) |
| Pressed suit button, no motion | Not in ASSIST / still waiting to connect | Re-enter suit ASSIST, check CM-connected LED |
| Robot stuck at 0°, won't swing (rare) | Homing "done" signal missed | Latest XM-Dev auto-continues after 3 s (patch included). If still stuck: suit STANDBY→ASSIST, or power-cycle |
| No data in PhAI Studio | Didn't press Connect / port held by another app | Select port + Connect in Studio, close other serial apps |

> 📌 **Do one rehearsal**: run ②–⑦ end-to-end before the real shoot, especially ⑥ (suit ASSIST → swing) 2–3 times.

---

## 4. Editing · Captions
- Make on-screen text/code large — readable on mobile.
- **Two things to pin as captions** (most common confusions):
  1. "ASSIST button is on the **robot (H10) body** (not the board)."
  2. "PhAI Studio needs you to press **Connect** before data starts."
- Don't rely on color alone for LED/status — add text too ("LED 1 = normal").
- Add chapter timestamps.

## 5. Final Pre-Shoot Check ✅
☐ Rev 2.0 board + built with latest XM-Dev firmware ☐ ST-Link recognized ☐ Suit ASSIST produces the swing ☐ PhAI Studio Connect shows data ☐ Robot mounted + stop person ☐ One rehearsal of ②–⑦

## 6. To Confirm Before Shooting (open items)
1. The actual PhAI Studio screen — the connect/Connect flow, and which graph shows the joint angles (confirm from Studio materials).
2. (Next video) the EMG sensor model — signal direction on contraction, and whether it stays within 0–3.3V.
