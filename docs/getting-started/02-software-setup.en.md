# 02 — Setting Up Your Development Environment

Installing STM32CubeIDE and downloading the SDK ZIP takes about 20 minutes including download time. This guide assumes you have already completed [01 Hardware Setup](01-hardware-setup.md).

Because the XM10 board's processor is a different architecture from your PC, you need a cross-compiler to convert C code into board-compatible machine code (`.elf`), a USB driver to communicate with the board, and a code editor. **STM32CubeIDE** — ST's free IDE — bundles all of this into a single installer. One install is all you need.

> Claude Code users can type `"Help me set up my environment"` to have the AI walk through every step on this page automatically → [Getting Started with Claude Code](00-claude-code-quickstart.md)

---

## What to Install

| Item | Required | Purpose | Download |
|------|----------|---------|---------|
| STM32CubeIDE v2.0.0+ | Required | Cross-compile C code for the board + debugger | [st.com](https://www.st.com/en/development-tools/stm32cubeide.html) (free ST account required) |
| Python 3.10+ | Required | Invoked automatically by the build's final step (creates the upload `.bin`) + log CSV conversion | [python.org](https://www.python.org/downloads/) (check **"Add python.exe to PATH"** during install) |
| 7-Zip | Optional | Extract ZIP files (fallback when the Windows built-in tool corrupts large archives) | [7-zip.org](https://www.7-zip.org/) |

CubeIDE installs the cross-compiler, the ST-Link USB driver, and the J-Link driver all at once. You do not need git — the SDK is distributed as a ZIP from GitHub Releases.

> **Why Python?** After each build, scripts bundled with the project run automatically to produce
> the firmware file for PhAI Studio upload (`XM10_x_x_x_x.bin`). If Python is not on your PATH,
> the build fails at that step — after installing, confirm `python --version` works in a
> command prompt.

---

## Steps

### 1. Install STM32CubeIDE

1. Go to the ST official page → select v2.0.0 or later → log in with your ST account → download the Windows installer.
2. Run the setup wizard and **confirm the following settings**:

| Setting | Recommended | Avoid |
|---------|-------------|-------|
| Installation path | `C:\ST\STM32CubeIDE_x.y.z\` (default) | Paths with non-ASCII characters or spaces (`C:\My Tools\`) |
| User privileges | Run as administrator | Standard user (some drivers may fail to install) |
| ST-Link driver | Install alongside CubeIDE ✅ | Unchecking this causes the board to go undetected |
| Antivirus software | Allow when prompted | Blocking halts the download |

3. After installation, verify:
```powershell
where STM32CubeIDE.exe
```
✅ A path printed to the console means success.

### 2. Download and Extract the SDK ZIP

The SDK is distributed as **per-revision ZIPs from GitHub Releases**. Download only the ZIP for your board revision — no git clone required.

**a. Identify your board revision**:
- Check the board label or packaging for `Rev 1.1` / `Rev 2.0`
- Visual check: if the board has an RJ45 Ethernet port, it is Rev 2.0; if not, it is Rev 1.1
- If still unsure → see the [docs/hardware/README.md](../hardware/README.md#보드-리비전-비교) revision comparison table

**b. Download from the Releases page**:

📦 https://github.com/AGR-AIFT/phai-x1-xm10-release/releases/latest

Under the Assets section, select your revision:
- **Rev 2.0** → `Rev2.0.zip`
- **Rev 1.1** → `Rev1.1.zip`

**c. Extract (PowerShell)**:

```powershell
New-Item -ItemType Directory -Force -Path C:\dev

# Extract the downloaded ZIP (assuming it was saved to the Downloads folder)
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\dev\ -Force
```

✅ Verify:
```powershell
Test-Path C:\dev\Extension_Module\.project      # CubeIDE project file
Test-Path C:\dev\Extension_Module\CLAUDE.md     # AI entry point
```
Both returning `True` means success. The extracted folder (`C:\dev\Extension_Module\`) is both the **project root that CubeIDE will import** and the **folder where you launch Claude Code**.

After extraction, the following items should all be present:

| Item | Role |
|------|------|
| `.project`, `.cproject`, `*.ld`, `startup_*.s` | CubeIDE project and build configuration |
| `CLAUDE.md` | AI tool auto-guide entry point (revision-specific) |
| `.claude/skills/` | Claude Code student onboarding and example troubleshooting skills |
| `docs/`, `Examples/` | Learning documentation + hands-on examples (Rev 2.0: 50 / Rev 1.1: 47, excludes Ex.40–42) |
| `Drivers/`, `XM_Apps/`, `XM_FW/` (includes `XM_API/`, `libXM_Lib.a`, `System/`, `Devices/`, `IOIF/`, `AGR_MW/`, `Services/`), `Core/` | SDK source code |
| `Middlewares/`, `FATFS/`, `LWIP/` (Rev 2.0 only) | HAL / CMSIS / STM32 middleware |

> 💡 If the top-level folder name inside the ZIP differs (e.g., `Extension_Module-Rev2.0/`), run `Get-ChildItem C:\dev\` to find the actual name and adjust subsequent paths accordingly. You can also rename it to `Extension_Module` with `Rename-Item` if needed.

### 3. Apply VS Code settings.json (Optional)

This step applies only if you use VS Code with clangd. The SDK includes a `.vscode/settings.json.template` (to avoid exposing absolute paths from the developer's machine).

```powershell
Copy-Item C:\dev\Extension_Module\.vscode\settings.json.template `
          C:\dev\Extension_Module\.vscode\settings.json
```

After copying, open `settings.json` and replace `<STM32CUBEIDE_INSTALL_PATH>` with your actual CubeIDE installation path. Skip this step if you are using CubeIDE only.

---

## Common Pitfalls

### Extraction Path

| Path example | Status |
|-------------|--------|
| `C:\dev\Extension_Module\` | Recommended |
| `C:\xm10\` | Recommended (shortest) |
| `D:\Projects\Extension_Module\` | OK |
| `C:\Users\username\Documents\GitHub\Extension_Module\` | Caution (non-ASCII characters + deep nesting) |
| `C:\Users\...\OneDrive - Company\...\Extension_Module\` | Risk of errors (exceeds MAX_PATH 260 + cloud sync conflicts) |

Follow three rules and you'll be fine:

1. Keep the path short and close to the drive root.
2. Avoid non-ASCII characters, spaces, and special characters.
3. Do not place the folder inside a cloud-synced directory such as OneDrive or iCloud (file lock conflicts).

For a permanent fix, see [Enable Windows Long Path](../troubleshooting.md#경로-길이-문제-windows-max_path).

### Installation / Download

- **"No eligible files" on the ST download page** — You are not logged in to your ST account. You can sign up for free with a school email address.
- **Antivirus blocks the installer** — Temporarily disable your antivirus and retry.
- **`where STM32CubeIDE.exe` returns nothing** — CubeIDE is not on the PATH. Launch it once from the Start menu, then try again.
- **GitHub Releases page does not load** — A corporate proxy or firewall is blocking it. Try from a different network or ask IT to whitelist `github.com`.
- **`Expand-Archive` drops files or hangs** — The Windows built-in extraction tool can fail on large ZIP files. Re-extract using [7-Zip](https://www.7-zip.org/).
- **Downloaded the wrong revision ZIP** — Check your board label again and re-download the correct ZIP. The two revisions are not interchangeable.

---

## Next Step

Once CubeIDE is installed and the SDK is extracted → [03 First Build & Run](03-first-build.md)
