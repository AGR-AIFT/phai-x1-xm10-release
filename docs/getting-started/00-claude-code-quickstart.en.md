# Getting Started with Claude Code

Even if embedded development and STM32CubeIDE are completely new to you, that's fine. An AI will walk you through everything — from installation all the way to blinking an LED on the board. Five minutes of reading is all it takes to understand how to get started.

> **You don't need the SDK ZIP yet.** Just install Claude Code, launch it from any empty folder, and share the GitHub URL — the AI will guide you through the download from there. See the [Starting from just a URL (before downloading)](#starting-from-just-a-url-before-downloading) section below for details.

---

## Why work alongside AI?

The manual guides (`01-hardware-setup.md` → `02-software-setup.md` → `03-first-build.md`) are thorough, but working through them alone has a few pain points:

- You have to keep track of where you are in the process yourself.
- When something goes wrong, you need to hunt down the troubleshooting docs separately.
- Easy-to-forget gotchas — like "no Korean characters in file paths" — are easy to overlook after a single read.

With Claude Code, the same content unfolds as a conversation. The AI won't move on until it has confirmed each step completed successfully, and if you get stuck it will help you diagnose the problem on the spot. Think of the difference between learning to drive from a textbook alone versus having an instructor sitting right next to you.

---

## What is Claude Code?

It is an AI coding tool built by Anthropic. It runs in a terminal or inside VS Code. Simply type `"처음 시작할게"` (or any of the trigger phrases below) and the AI automatically reads this SDK's `CLAUDE.md` and `.claude/skills/`, then begins guiding you step by step.

- Install: https://claude.com/claude-code
- Free trial available with an Anthropic account

### What the AI does in this SDK

| Phase | What the AI does | What you do |
|-------|-----------------|-------------|
| 0 | Runs OS / file-path / permission checks | Review the results |
| 1 | Opens the STM32CubeIDE download page automatically, verifies installation | Run the installer wizard |
| 2 | Guides SDK ZIP download → extraction → CubeIDE import | Follow the clicks |
| 3 | Walks through the build command, verifies `.elf` is generated | Press `Ctrl + B` |
| 4 | Confirms ST-Link is detected, guides the flash step | Connect the board and cable |
| 5 | Asks whether the LED lit up | Look at the board and answer |
| 6 | Hands off to Ex.00 | Run your first code experiment |

Full details for each phase are in `.claude/skills/student-onboard/SKILL.md` and `phases/01~06-*.md`.

---

## How to use it (3 steps)

### 1. Install Claude Code

Follow the instructions on the official site. Windows, macOS, and Linux are all supported.
- https://claude.com/claude-code

### 2. Download the SDK ZIP and extract it

Go to the GitHub Releases page and download only the ZIP for your board revision:

📦 https://github.com/AGR-EXO/Extension_Module/releases/latest

- **Rev 2.0 board** → `Rev2.0.zip` (most users)
- **Rev 1.1 board** → `Rev1.1.zip`

```powershell
# Recommended: a short path with no Korean characters
New-Item -ItemType Directory -Force -Path C:\dev

# Extract the downloaded ZIP (assuming it is in your Downloads folder)
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\dev\ -Force

cd C:\dev\Extension_Module
```

The extracted folder (`C:\dev\Extension_Module\`) is the **single entry point for everything**:

- ✅ **Claude Code working directory** — launching `claude` here auto-loads this folder's `CLAUDE.md` (revision-specific) and `.claude/skills/`
- ✅ **CubeIDE import root** — import this folder directly (the `.project` file sits at the folder root)
- ✅ **Code editing folder** — `XM_Apps/Control_Task/control_task.c` is immediately accessible within the same folder

> 💡 If the top-level folder inside the ZIP has a different name (e.g. `Extension_Module-Rev2.0/`), run `Get-ChildItem C:\dev\` to check the actual name and adjust the `cd` command accordingly.

### 3. Launch Claude Code and trigger the guided setup

From the extracted folder:
```powershell
claude
```

Once Claude Code starts, `CLAUDE.md` is loaded automatically. Type one of the following:

- `처음 시작할게`
- `환경 구축 도와줘`
- `XM10 시작하려고 해`
- `/student-onboard`

→ The AI will walk through the process from Phase 0, with a ✅ checkpoint at every step.

### Starting from just a URL (before downloading)

If you only have the GitHub URL (`https://github.com/AGR-EXO/Extension_Module`) — for example, from a class handout:

1. Install Claude Code and launch it from any directory.
2. Type the following to the AI:
   ```
   https://github.com/AGR-EXO/Extension_Module — I want to start from scratch with this
   ```
3. The AI will use `WebFetch` to read the page, ask "Which board revision do you have?", and then guide you through downloading the correct ZIP from the Releases page.

---

## Common sticking points

- **Claude Code is hard to install** — Check the Anthropic official docs for OS-specific installation guides. On a school or company PC, you may need administrator privileges.
- **The AI doesn't understand Korean?** — Korean input works fine. Just make sure the commands themselves (`claude`, `Expand-Archive`, etc.) are typed in English.
- **Permission prompts (Allow / Deny) look scary** — The permissions the AI requests are limited to: opening a browser (`Start-Process`), verifying installations (`where`), and extracting archives (`Expand-Archive`). These are all local, read-only operations.
- **I want to stop the AI and do it manually** — Say `"여기까지 할게"` at any time and the AI will stop. The manual procedure is documented identically starting from [01-hardware-setup.md](01-hardware-setup.md).
- **I forgot where I left off** — Even after closing a Claude Code session, a `~/.xm10-onboard-done` sentinel file keeps track of your progress so the AI can pick up where you left off. To restart from the very beginning, delete the sentinel file (see Phase 6 for details).

---

## Next steps

- **With AI guidance:** Launch Claude Code and type `"처음 시작할게"`
- **Manual setup:** [01 Hardware Setup](01-hardware-setup.md) → [02 Software Setup](02-software-setup.md) → [03 First Build](03-first-build.md)
- **Already set up:** [Ex.00 Quick Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) → [Learning Path](../tutorials/README.md)
