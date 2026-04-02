#pragma once
#include <cstdint>
#include <cstring>

// ============================================================
// A46 ARCHITECTURE CONSTANTS (UPGRADED TO 32-BIT)
// ============================================================
static const int A46_MEM_SIZE       = 356 * 1024; // 356KB
static const int A46_DISP_W         = 30;
static const int A46_DISP_H         = 40;
static const int A46_DISP_SIZE      = A46_DISP_W * A46_DISP_H; // 1200

// Memory Map:
// 0x00000 - 0x589FF: User RAM (Program & Data)
// 0x58A00 - 0x58EAF: Display VRAM (1200 bytes)
// 0x58EB0 - 0x58EB3: D-Pad Input
// 0x58EB4:           VSync Port
// 0x58FFF:           Stack Start (Grows down)

static const uint32_t A46_DISP_START = 0x58A00;
static const uint32_t A46_DISP_END   = A46_DISP_START + A46_DISP_SIZE - 1;
static const uint32_t A46_STACK_INIT = 0x58FFF;
static const int A46_MEM_ROW_BYTES  = 16;
static const int A46_MEM_ROWS       = A46_MEM_SIZE / A46_MEM_ROW_BYTES;

// Input registers (memory-mapped D-pad)
static const uint32_t A46_INPUT_UP    = 0x58EB0;
static const uint32_t A46_INPUT_DOWN  = 0x58EB1;
static const uint32_t A46_INPUT_LEFT  = 0x58EB2;
static const uint32_t A46_INPUT_RIGHT = 0x58EB3;

// VSync port
static const uint32_t A46_SYNC_PORT   = 0x58EB4;

// ============================================================
// REGISTERS
// ============================================================
enum RegId { REG_A = 0, REG_B, REG_C, REG_D, REG_COUNT };

static const char* RegNames[] = { "A", "B", "C", "D" };

// ============================================================
// OPCODES
// ============================================================
enum Opcode : uint8_t {
    OP_NOP          = 0x00, // 1 byte
    OP_MOV_RI       = 0x01, // MOV reg, imm32        (6 bytes: op + reg + 4)
    OP_MOV_RR       = 0x02, // MOV reg, reg           (3 bytes)
    OP_LOAD_RA      = 0x03, // LOAD reg, [imm32]      (6 bytes)
    OP_STORE_AR     = 0x04, // STORE [imm32], reg      (6 bytes)
    OP_ADD_RR       = 0x05, // ADD reg, reg            (3 bytes)
    OP_SUB_RR       = 0x06, // SUB reg, reg            (3 bytes)
    OP_MUL_RR       = 0x07, // MUL reg, reg            (3 bytes)
    OP_DIV_RR       = 0x08, // DIV reg, reg            (3 bytes)
    OP_AND_RR       = 0x09, // AND reg, reg            (3 bytes)
    OP_OR_RR        = 0x0A, // OR  reg, reg            (3 bytes)
    OP_XOR_RR       = 0x0B, // XOR reg, reg            (3 bytes)
    OP_NOT_R        = 0x0C, // NOT reg                 (2 bytes)
    OP_CMP_RR       = 0x0D, // CMP reg, reg            (3 bytes)
    OP_JMP          = 0x0E, // JMP addr32              (5 bytes: op + 4)
    OP_JZ           = 0x0F, // JZ  addr32              (5 bytes)
    OP_JNZ          = 0x10, // JNZ addr32              (5 bytes)
    OP_JG           = 0x11, // JG  addr32              (5 bytes)
    OP_JL           = 0x12, // JL  addr32              (5 bytes)
    OP_PUSH         = 0x13, // PUSH reg                (2 bytes)
    OP_POP          = 0x14, // POP  reg                (2 bytes)
    OP_CALL         = 0x15, // CALL addr32             (5 bytes)
    OP_RET          = 0x16, // RET                     (1 byte)
    OP_HLT          = 0x17, // HLT                     (1 byte)
    OP_ADD_RI       = 0x18, // ADD reg, imm32          (6 bytes)
    OP_SUB_RI       = 0x19, // SUB reg, imm32          (6 bytes)
    OP_LOAD_RR      = 0x1A, // LOAD reg, [reg]         (3 bytes)
    OP_STORE_RR     = 0x1B, // STORE [reg], reg        (3 bytes)
    OP_INC          = 0x1C, // INC reg                 (2 bytes)
    OP_DEC          = 0x1D, // DEC reg                 (2 bytes)
    OP_JGE          = 0x1E, // JGE addr32              (5 bytes)
    OP_JLE          = 0x1F, // JLE addr32              (5 bytes)
    OP_CMP_RI       = 0x20, // CMP reg, imm32          (6 bytes)
    OP_SHL          = 0x21, // SHL reg, reg            (3 bytes)
    OP_SHR          = 0x22, // SHR reg, reg            (3 bytes)
    OP_MOD_RR       = 0x23, // MOD reg, reg            (3 bytes)
};

// ============================================================
// CPU STATE
// ============================================================
struct A46_CPU {
    uint32_t reg[REG_COUNT];
    uint32_t ip;
    uint32_t sp;
    bool flagZ;     // zero
    bool flagS;     // sign (negative / underflow)
    bool halted;
    bool yielded;   // for VSync swapping
    uint8_t mem[A46_MEM_SIZE];

    void reset() {
        memset(reg, 0, sizeof(reg));
        ip = 0;
        sp = A46_STACK_INIT;
        flagZ = false;
        flagS = false;
        halted = false;
        yielded = false;
        memset(mem, 0, sizeof(mem));
    }

    uint8_t  fetch8()  { return mem[ip++]; }
    uint16_t fetch16() { uint16_t lo = mem[ip++]; uint16_t hi = mem[ip++]; return lo | (hi << 8); }
    uint32_t fetch32() {
        uint32_t v0 = mem[ip++]; uint32_t v1 = mem[ip++];
        uint32_t v2 = mem[ip++]; uint32_t v3 = mem[ip++];
        return v0 | (v1 << 8) | (v2 << 16) | (v3 << 24);
    }

    void push32(uint32_t v) {
        mem[sp] = v & 0xFF; mem[sp-1] = (v >> 8) & 0xFF;
        mem[sp-2] = (v >> 16) & 0xFF; mem[sp-3] = (v >> 24) & 0xFF;
        sp -= 4;
    }
    uint32_t pop32() {
        sp += 4;
        return mem[sp] | (mem[sp-1] << 8) | (mem[sp-2] << 16) | (mem[sp-3] << 24);
    }

    void setFlags(uint32_t r) { flagZ = (r == 0); flagS = (r & 0x80000000) != 0; }

    // Execute one instruction. Returns false if halted or error.
    bool step() {
        if (halted) return false;
        uint8_t op = fetch8();
        uint8_t r1, r2;
        uint32_t imm, addr, result;

        switch (op) {
        case OP_NOP: break;

        case OP_MOV_RI:
            r1 = fetch8(); imm = fetch32();
            reg[r1] = imm;
            break;
        case OP_MOV_RR:
            r1 = fetch8(); r2 = fetch8();
            reg[r1] = reg[r2];
            break;

        case OP_LOAD_RA:
            r1 = fetch8(); addr = fetch32();
            if (addr < A46_MEM_SIZE) reg[r1] = mem[addr];
            else reg[r1] = 0;
            break;
        case OP_STORE_AR:
            addr = fetch32(); r1 = fetch8();
            if (addr < A46_MEM_SIZE) mem[addr] = (uint8_t)(reg[r1] & 0xFF);
            if (addr == A46_SYNC_PORT) yielded = true;
            break;
        case OP_LOAD_RR:
            r1 = fetch8(); r2 = fetch8();
            addr = reg[r2];
            if (addr < A46_MEM_SIZE) reg[r1] = mem[addr];
            else reg[r1] = 0;
            break;
        case OP_STORE_RR:
            r1 = fetch8(); r2 = fetch8();
            addr = reg[r1];
            if (addr < A46_MEM_SIZE) mem[addr] = (uint8_t)(reg[r2] & 0xFF);
            if (addr == A46_SYNC_PORT) yielded = true;
            break;

        case OP_ADD_RR:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] + reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_ADD_RI:
            r1 = fetch8(); imm = fetch32();
            result = reg[r1] + imm; reg[r1] = result; setFlags(result);
            break;
        case OP_SUB_RR:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] - reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_SUB_RI:
            r1 = fetch8(); imm = fetch32();
            result = reg[r1] - imm; reg[r1] = result; setFlags(result);
            break;
        case OP_MUL_RR:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] * reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_DIV_RR:
            r1 = fetch8(); r2 = fetch8();
            if (reg[r2] == 0) { halted = true; return false; }
            result = reg[r1] / reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_MOD_RR:
            r1 = fetch8(); r2 = fetch8();
            if (reg[r2] == 0) { halted = true; return false; }
            result = reg[r1] % reg[r2]; reg[r1] = result; setFlags(result);
            break;

        case OP_AND_RR:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] & reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_OR_RR:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] | reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_XOR_RR:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] ^ reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_NOT_R:
            r1 = fetch8();
            result = ~reg[r1]; reg[r1] = result; setFlags(result);
            break;
        case OP_INC:
            r1 = fetch8();
            reg[r1]++; setFlags(reg[r1]);
            break;
        case OP_DEC:
            r1 = fetch8();
            reg[r1]--; setFlags(reg[r1]);
            break;
        case OP_SHL:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] << reg[r2]; reg[r1] = result; setFlags(result);
            break;
        case OP_SHR:
            r1 = fetch8(); r2 = fetch8();
            result = reg[r1] >> reg[r2]; reg[r1] = result; setFlags(result);
            break;

        case OP_CMP_RR:
            r1 = fetch8(); r2 = fetch8();
            setFlags(reg[r1] - reg[r2]);
            break;
        case OP_CMP_RI:
            r1 = fetch8(); imm = fetch32();
            setFlags(reg[r1] - imm);
            break;

        case OP_JMP:
            ip = fetch32();
            break;
        case OP_JZ:
            addr = fetch32();
            if (flagZ) ip = addr;
            break;
        case OP_JNZ:
            addr = fetch32();
            if (!flagZ) ip = addr;
            break;
        case OP_JG:
            addr = fetch32();
            if (!flagZ && !flagS) ip = addr;
            break;
        case OP_JL:
            addr = fetch32();
            if (flagS) ip = addr;
            break;
        case OP_JGE:
            addr = fetch32();
            if (!flagS) ip = addr;
            break;
        case OP_JLE:
            addr = fetch32();
            if (flagZ || flagS) ip = addr;
            break;

        case OP_PUSH:
            r1 = fetch8();
            push32(reg[r1]);
            break;
        case OP_POP:
            r1 = fetch8();
            reg[r1] = pop32();
            break;
        case OP_CALL:
            addr = fetch32();
            push32(ip);
            ip = addr;
            break;
        case OP_RET:
            ip = pop32();
            break;
        case OP_HLT:
            halted = true;
            return false;

        default:
            halted = true;
            return false;
        }
        return true;
    }
};
