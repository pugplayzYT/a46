# 🚀 A46 Computer Emulator - 32-Bit V2 Edition

![GitHub repo size](https://img.shields.io/github/repo-size/pugplayzYT/a46?style=for-the-badge&logo=git&color=orange)
![GitHub language count](https://img.shields.io/github/languages/count/pugplayzYT/a46?style=for-the-badge&logo=github&color=blue)
![GitHub top language](https://img.shields.io/github/languages/top/pugplayzYT/a46?style=for-the-badge&logo=c%2B%2B&color=blue)
![GitHub last commit](https://img.shields.io/github/last-commit/pugplayzYT/a46?style=for-the-badge&logo=git&color=green)

![A46 Emulator Logo](logo.png)

Welcome to the **A46 Computer Emulator**, a custom high-performance 32-bit architecture designed for retro enthusiasts, assembly lovers, and educational exploration. 

The A46 system combines the simplicity of early 80s arcade hardware with the power of a modern 32-bit CPU, featuring a dedicated high-refresh pixel display and real-time D-pad input.

---

## 🌟 Key Features

- **Custom 32-Bit CPU**: A unique ISA with 32-bit registers and 4-byte stack operations.
- **High-Speed Execution**: Capable of executing up to **50,000 instructions per frame** at 60 FPS.
- **Embedded Assembler**: Write, assemble, and run your code instantly within the emulator.
- **Visual Debugging Tools**:
  - Live **Registers Panel** showing A, B, C, D, IP, and SP.
  - Real-time **Memory Viewer** with hex/char display and IP highlighting.
  - Integrated **Status Bar** for instruction-level stepping feedback.
- **Retro Graphics**: A 30x40 pixel grid with a rich 256-color palette.
- **Memory-Mapped I/O**: Direct access to D-pad buttons and an advanced VSync port for smooth animations.

---

## 🛠️ Getting Started

### Prerequisites
- Windows OS (Windows 10/11 recommended)
- A C++ Compiler (MinGW/GCC or MSVC)

### Building from Source
Run the provided `build.bat` script to compile the emulator:
```batch
build.bat
```
This will generate `a46.exe` in the root directory.

### Running the Emulator
Launch `a46.exe`. You can immediately start writing code in the "ASSEMBLY" editor or load an existing `.asm` file from the **File** menu.

---

## 🏗️ Architecture Overview

The A46 is a Harward-ish architecture where code and data share a unified memory space.

### Central Processing Unit (CPU)
- **General Purpose Registers**: `A`, `B`, `C`, `D` (all 32-bit).
- **Special Registers**:
  - `IP` (Instruction Pointer): Points to the current instruction.
  - `SP` (Stack Pointer): Points to the current top of the stack (initializes at `0x58FFF`).
- **Flags**:
  - `Z` (Zero Flag): Set if the last ALU result was zero.
  - `S` (Sign Flag): Set if the last ALU result was negative.

### Memory Map (356KB Total)
| Address Range | Description |
| :--- | :--- |
| **0x00000 - 0x589FF** | **User RAM**: Code starts at `0x0000`. Use `0x1000+` for variables. |
| **0x58A00 - 0x58EAF** | **Video RAM (VRAM)**: 1200 bytes (30x40 pixels). |
| **0x58EB0 - 0x58EB3** | **D-Pad Input**: Memory-mapped buttons (Up, Down, Left, Right). |
| **0x58EB4** | **VSync Port**: Write here to wait for the next frame. |
| **0x58FFF** | **Stack Top**: Stack grows downward toward RAM. |

---

## 📜 Instruction Set (ISA)

The A46 uses a custom opcode set designed for efficiency and ease of assembly.

### Data Movement
- `MOV reg, value`: Load a 32-bit constant into a register.
- `MOV reg, reg`: Copy value between registers.
- `LOAD reg, [addr]`: Read 1 byte from a literal address into a register (zero-extended).
- `LOAD reg, [reg]`: Read 1 byte from the address pointed to by a register.
- `STORE [addr], reg`: Write the low byte (bits 0-7) of a register to memory.
- `STORE [reg], reg`: Write the low byte to the address pointed to by a register.
- `PUSH / POP reg`: Stack operations (handle full 32-bit values).

### Arithmetic & Logic
- `ADD / SUB / MUL / DIV / MOD`: Standard ALU operations.
- `INC / DEC`: Increment or decrement a register.
- `AND / OR / XOR / NOT`: Bitwise operations.
- `SHL / SHR`: Logical bit shifts.

### Control Flow
- `CMP reg, reg/val`: Compare values and set flags.
- `JMP label`: Unconditional jump.
- `JZ / JNZ / JG / JL / JGE / JLE`: Conditional jumps based on Z and S flags.
- `CALL / RET`: Subroutine management using the stack.
- `HLT`: Stop execution and halt the CPU.

---

## 🎮 Input & Output

### D-Pad Control
Button states are mapped to high memory. A value of `1` means pressed, `0` means released.
```assembly
LOAD D, [0x58EB0] ; Check UP
LOAD D, [0x58EB1] ; Check DOWN
LOAD D, [0x58EB2] ; Check LEFT
LOAD D, [0x58EB3] ; Check RIGHT
```

### Display & VRAM
The screen is a 30x40 grid. Each byte in VRAM corresponds to a pixel color.
- **Address Formula**: `0x58A00 + (Y * 30) + X`
- **Colors**: Uses a standard 256-color palette. `0` is black, `1` is white, `2` is red, etc.

### VSync Synchronization
To prevent screen tearing and keep games running at 60 FPS, always sync with the display at the end of your loop:
```assembly
loop:
    ; ... your game logic ...
    MOV D, 1
    STORE [0x58EB4], D ; Pause until next frame
    JMP loop
```

---

## ⚡ Pro Tips for Developers

> [!TIP]
> **Use the Stack**: Since you only have 4 registers, use `PUSH` and `POP` extensively to save temporary values during calculation.

> [!IMPORTANT]
> **Code Placement**: Your code starts at `0x0000`. Be careful not to use `STORE` on low addresses, or you might overwrite your own program code! We recommend placing your data/variables starting at `0x1000`.

> [!WARNING]
> **Store/Load 8-Bit**: Remember that `LOAD` and `STORE` only handle **8 bits** at a time. If you need to save a full 32-bit register to memory, you'll need 4 `STORE` operations or a clever loop.

---

## 🕹️ Example: Moving Pixel
Paste this into the emulator to see it in action!
```assembly
    MOV B, 15 ; Start X
    MOV C, 20 ; Start Y
    MOV D, 3  ; Green color

loop:
    ; Clear old position
    CALL calc_addr
    PUSH D
    MOV D, 0
    STORE [A], D
    POP D

    ; Move right
    INC B
    CMP B, 30
    JNZ draw
    MOV B, 0 ; Wrap around

draw:
    CALL calc_addr
    STORE [A], D
    
    ; VSync
    MOV D, 1
    STORE [0x58EB4], D
    JMP loop

calc_addr:
    MOV A, 0x58A00
    PUSH B
    PUSH C
ca_m:
    CMP C, 0
    JZ ca_a
    ADD A, 30
    DEC C
    JMP ca_m
ca_a:
    ADD A, B
    POP C
    POP B
    RET
```

---

*Crafted with 💖 by the A46 Development Team. Happy coding!*
