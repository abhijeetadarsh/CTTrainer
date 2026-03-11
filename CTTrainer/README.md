# CTTrainer – Generic Cheat Engine CT Trainer

A standalone Windows trainer that loads any **Cheat Engine `.CT` file** and lets you inspect, write, and freeze values in **any game process** — 32-bit or 64-bit.

Built with **ImGui + DirectX 11 + Win32**.

---

## Project Structure
```
CTTrainer/
├── CTTrainer.sln
├── CTTrainer/
│   ├── src/
│   │   ├── main.cpp          ← Entry point, D3D11 window
│   │   ├── Memory.cpp        ← Process attach, universal pointer resolution (32/64-bit)
│   │   ├── CheatManager.cpp  ← Freeze threads, cheat logic
│   │   ├── CTParser.cpp      ← Cheat Engine .CT file parser
│   │   ├── UI.cpp            ← All ImGui rendering
│   │   ├── Theme.cpp         ← Theme / palette system
│   │   ├── Widgets.cpp       ← Custom widget library
│   │   └── Logger.cpp        ← In-memory debug log
│   ├── include/
│   │   ├── Memory.h
│   │   ├── CheatManager.h
│   │   ├── CTParser.h
│   │   ├── UI.h
│   │   ├── Theme.h
│   │   ├── Widgets.h
│   │   └── Logger.h
│   └── CTTrainer.vcxproj
├── imgui/                    ← ImGui source files (unchanged)
│   ├── include/
│   └── src/
└── config/
    └── *.CT                  ← Your Cheat Engine tables
```

---

## Visual Studio Setup

### 1. Add all files to project
- Right-click project → Add → Existing Item
- Add all files from `src/`
- Create a filter "imgui" and add all files from `imgui/src/`
- (include/ headers are found automatically)

### 2. Include Directories
Project → Properties → C/C++ → General → Additional Include Directories:
```
$(SolutionDir)imgui\include
$(ProjectDir)include
```

### 3. Linker
Project → Properties → Linker → Input → Additional Dependencies:
```
d3d11.lib
dxgi.lib
```

### 4. Subsystem
Linker → System → SubSystem → **Windows**

### 5. Run as Administrator
Always run the built `.exe` as Administrator (required for `OpenProcess`).

---

## How to Use

1. **Build** in Release x64
2. **Run as Administrator**
3. Launch your target game
4. Click **Browse** → select your `.CT` file from `config/`
5. Click **Load CT**
6. Type the game's process name (e.g. `FarCry3.exe`, `RDR2.exe`, `Cyberpunk2077.exe`)
7. **32-bit checkbox** — tick it for older 32-bit games, untick for modern 64-bit games
8. Click **Attach**
9. Toggle individual cheats or use **FREEZE ALL**

---

## 32-bit vs 64-bit

| Game era | Setting |
|---|---|
| Old games (pre-2012 era, c. 32-bit) | ✅ Tick **32-bit process** |
| Modern games (64-bit executables) | ☐ Untick **32-bit process** |

The trainer auto-detects nothing here — **you must set this correctly** for pointer chains to resolve.

---

## CT File Format

Your `.CT` export from Cheat Engine should have this structure:
```xml
<CheatEntry>
  <Description>"Health"</Description>
  <VariableType>4 Bytes</VariableType>
  <Address>Game.exe+0x1A2B3C</Address>
  <Offsets>
    <Offset>4C</Offset>
    <Offset>10</Offset>
  </Offsets>
</CheatEntry>
```

The trainer auto-detects cheat type by description keyword:
| Keyword in description | Default freeze value |
|------------------------|----------------------|
| health / stamina       | 100                  |
| ammo                   | 999                  |
| money / cash           | 999999               |
| speed                  | 2.0 (float)          |

You can override the value live in the trainer UI.