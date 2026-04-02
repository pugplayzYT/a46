#pragma once
#include "cpu.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cstdio>

// ============================================================
// A46 ASSEMBLER — Upgraded to 32-bit
// ============================================================

struct AsmResult {
    bool success;
    std::string error;
    int errorLine; // 1-based, -1 if N/A
};

// --- Utility helpers ---
static std::string asmTrim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string asmUpper(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)toupper((unsigned char)c);
    return r;
}

static int parseReg(const std::string& s) {
    std::string u = asmUpper(asmTrim(s));
    if (u == "A") return REG_A;
    if (u == "B") return REG_B;
    if (u == "C") return REG_C;
    if (u == "D") return REG_D;
    return -1;
}

static bool parseNumber(const std::string& s, uint32_t& out) {
    std::string t = asmTrim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    unsigned long v;
    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
        v = strtoul(t.c_str(), &end, 16);
    else if (t.size() > 1 && t[0] == '0' && t[1] == 'b')
        v = strtoul(t.c_str() + 2, &end, 2);
    else
        v = strtoul(t.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') return false;
    out = (uint32_t)v;
    return true;
}

// Is the operand a memory reference like [A] or [0x58A00]?
static bool isMemRef(const std::string& s) {
    std::string t = asmTrim(s);
    return t.size() >= 3 && t.front() == '[' && t.back() == ']';
}

static std::string stripBrackets(const std::string& s) {
    std::string t = asmTrim(s);
    return t.substr(1, t.size() - 2);
}

// Split "A, B" into {"A", "B"}
static std::vector<std::string> splitOperands(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    for (char c : s) {
        if (c == '[') depth++;
        if (c == ']') depth--;
        if (c == ',' && depth == 0) {
            out.push_back(asmTrim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!asmTrim(cur).empty()) out.push_back(asmTrim(cur));
    return out;
}

// ============================================================
// Instruction size calculator (32-bit UPGRADE)
// ============================================================
static int instrSize(const std::string& mnemonic, const std::vector<std::string>& ops) {
    std::string mn = asmUpper(mnemonic);

    if (mn == "NOP" || mn == "RET" || mn == "HLT") return 1;
    if (mn == "NOT" || mn == "PUSH" || mn == "POP" || mn == "INC" || mn == "DEC") return 2;

    if (mn == "MOV") {
        if (ops.size() == 2 && parseReg(ops[0]) >= 0 && parseReg(ops[1]) >= 0) return 3;
        return 6; // MOV reg, imm32 (1 + 1 + 4)
    }
    if (mn == "LOAD") {
        if (ops.size() == 2 && isMemRef(ops[1])) {
            std::string inner = stripBrackets(ops[1]);
            if (parseReg(inner) >= 0) return 3; // LOAD reg, [reg]
            return 6; // LOAD reg, [addr32]
        }
        return 6;
    }
    if (mn == "STORE") {
        if (ops.size() == 2 && isMemRef(ops[0])) {
            std::string inner = stripBrackets(ops[0]);
            if (parseReg(inner) >= 0) return 3; // STORE [reg], reg
            return 6; // STORE [addr32], reg
        }
        return 6;
    }
    if (mn == "ADD" || mn == "SUB") {
        if (ops.size() == 2 && parseReg(ops[1]) >= 0) return 3; // reg, reg
        return 6; // reg, imm32
    }
    if (mn == "CMP") {
        if (ops.size() == 2 && parseReg(ops[1]) >= 0) return 3;
        return 6;
    }
    // 3-byte: two-reg ALU ops
    if (mn == "MUL" || mn == "DIV" || mn == "MOD" ||
        mn == "AND" || mn == "OR"  || mn == "XOR" ||
        mn == "SHL" || mn == "SHR") return 3;
    // 5-byte: jumps/call (1 + 4)
    if (mn == "JMP" || mn == "JZ"  || mn == "JNZ" ||
        mn == "JG"  || mn == "JL"  || mn == "JGE" ||
        mn == "JLE" || mn == "CALL") return 5;

    return -1; // unknown
}

// ============================================================
// ASSEMBLE — writes machine code into cpu.mem starting at 0
// ============================================================
static AsmResult assemble(const std::string& source, A46_CPU& cpu) {
    AsmResult res = { true, "", -1 };

    // Split source into lines
    std::vector<std::string> lines;
    {
        std::istringstream ss(source);
        std::string ln;
        while (std::getline(ss, ln)) lines.push_back(ln);
    }

    // === PASS 1: collect labels and instruction sizes ===
    struct LineInfo {
        int srcLine;        
        std::string label;  
        std::string mnem;
        std::vector<std::string> ops;
        uint32_t addr;
    };
    std::vector<LineInfo> parsed;
    std::map<std::string, uint32_t> labels;
    uint32_t pc = 0;

    for (int i = 0; i < (int)lines.size(); i++) {
        std::string line = lines[i];
        size_t sc = line.find(';');
        if (sc != std::string::npos) line = line.substr(0, sc);
        line = asmTrim(line);
        if (line.empty()) continue;

        if (line.back() == ':') {
            std::string lbl = asmUpper(asmTrim(line.substr(0, line.size() - 1)));
            if (labels.count(lbl)) {
                res.success = false;
                res.error = "Duplicate label: " + lbl;
                res.errorLine = i + 1;
                return res;
            }
            labels[lbl] = pc;
            continue;
        }

        LineInfo li;
        li.srcLine = i + 1;
        li.addr = pc;

        size_t sp = line.find_first_of(" \t");
        if (sp == std::string::npos) {
            li.mnem = asmUpper(line);
        } else {
            li.mnem = asmUpper(line.substr(0, sp));
            std::string rest = asmTrim(line.substr(sp));
            li.ops = splitOperands(rest);
        }

        int sz = instrSize(li.mnem, li.ops);
        if (sz < 0) {
            res.success = false;
            char buf[128];
            sprintf(buf, "Unknown instruction: %s", li.mnem.c_str());
            res.error = buf;
            res.errorLine = i + 1;
            return res;
        }
        pc += (uint32_t)sz;
        parsed.push_back(li);
    }

    // === PASS 2: emit bytecode ===
    auto emitByte = [&](uint32_t& addr, uint8_t v) { cpu.mem[addr++] = v; };
    auto emit32   = [&](uint32_t& addr, uint32_t v) {
        cpu.mem[addr++] = v & 0xFF;
        cpu.mem[addr++] = (v >> 8) & 0xFF;
        cpu.mem[addr++] = (v >> 16) & 0xFF;
        cpu.mem[addr++] = (v >> 24) & 0xFF;
    };

    auto resolveAddr = [&](const std::string& s, uint32_t& out, int srcLine) -> bool {
        std::string t = asmUpper(asmTrim(s));
        if (labels.count(t)) { out = labels[t]; return true; }
        if (parseNumber(asmTrim(s), out)) return true;
        res.success = false;
        char buf[128];
        sprintf(buf, "Cannot resolve '%s'", s.c_str());
        res.error = buf;
        res.errorLine = srcLine;
        return false;
    };

    for (auto& li : parsed) {
        uint32_t addr = li.addr;
        const auto& mn = li.mnem;
        const auto& ops = li.ops;

        #define NEED_OPS(n) if ((int)ops.size() < n) { \
            res.success = false; res.error = mn + " needs " #n " operand(s)"; \
            res.errorLine = li.srcLine; return res; }
        #define GET_REG(idx) parseReg(ops[idx])

        if (mn == "NOP") { emitByte(addr, OP_NOP); }
        else if (mn == "HLT") { emitByte(addr, OP_HLT); }
        else if (mn == "RET") { emitByte(addr, OP_RET); }

        else if (mn == "MOV") {
            NEED_OPS(2);
            int rd = GET_REG(0);
            int rs = GET_REG(1);
            if (rd < 0) { res = {false, "MOV: first operand must be register", li.srcLine}; return res; }
            if (rs >= 0) {
                emitByte(addr, OP_MOV_RR); emitByte(addr, (uint8_t)rd); emitByte(addr, (uint8_t)rs);
            } else {
                uint32_t v;
                if (!resolveAddr(ops[1], v, li.srcLine)) return res;
                emitByte(addr, OP_MOV_RI); emitByte(addr, (uint8_t)rd); emit32(addr, v);
            }
        }
        else if (mn == "LOAD") {
            NEED_OPS(2);
            int rd = GET_REG(0);
            if (rd < 0) { res = {false, "LOAD: first operand must be register", li.srcLine}; return res; }
            if (!isMemRef(ops[1])) { res = {false, "LOAD: second operand must be [addr] or [reg]", li.srcLine}; return res; }
            std::string inner = stripBrackets(ops[1]);
            int ri = parseReg(inner);
            if (ri >= 0) {
                emitByte(addr, OP_LOAD_RR); emitByte(addr, (uint8_t)rd); emitByte(addr, (uint8_t)ri);
            } else {
                uint32_t v;
                if (!resolveAddr(inner, v, li.srcLine)) return res;
                emitByte(addr, OP_LOAD_RA); emitByte(addr, (uint8_t)rd); emit32(addr, v);
            }
        }
        else if (mn == "STORE") {
            NEED_OPS(2);
            int rv = GET_REG(1);
            if (rv < 0) { res = {false, "STORE: second operand must be register", li.srcLine}; return res; }
            if (!isMemRef(ops[0])) { res = {false, "STORE: first operand must be [addr] or [reg]", li.srcLine}; return res; }
            std::string inner = stripBrackets(ops[0]);
            int ri = parseReg(inner);
            if (ri >= 0) {
                emitByte(addr, OP_STORE_RR); emitByte(addr, (uint8_t)ri); emitByte(addr, (uint8_t)rv);
            } else {
                uint32_t v;
                if (!resolveAddr(inner, v, li.srcLine)) return res;
                emitByte(addr, OP_STORE_AR); emit32(addr, v); emitByte(addr, (uint8_t)rv);
            }
        }
        else if (mn == "ADD" || mn == "SUB") {
            NEED_OPS(2);
            int rd = GET_REG(0);
            if (rd < 0) { res = {false, mn + ": first operand must be register", li.srcLine}; return res; }
            int rs = GET_REG(1);
            if (rs >= 0) {
                emitByte(addr, mn == "ADD" ? OP_ADD_RR : OP_SUB_RR);
                emitByte(addr, (uint8_t)rd); emitByte(addr, (uint8_t)rs);
            } else {
                uint32_t v;
                if (!resolveAddr(ops[1], v, li.srcLine)) return res;
                emitByte(addr, mn == "ADD" ? OP_ADD_RI : OP_SUB_RI);
                emitByte(addr, (uint8_t)rd); emit32(addr, v);
            }
        }
        else if (mn == "CMP") {
            NEED_OPS(2);
            int rd = GET_REG(0);
            if (rd < 0) { res = {false, "CMP: first operand must be register", li.srcLine}; return res; }
            int rs = GET_REG(1);
            if (rs >= 0) {
                emitByte(addr, OP_CMP_RR); emitByte(addr, (uint8_t)rd); emitByte(addr, (uint8_t)rs);
            } else {
                uint32_t v;
                if (!resolveAddr(ops[1], v, li.srcLine)) return res;
                emitByte(addr, OP_CMP_RI); emitByte(addr, (uint8_t)rd); emit32(addr, v);
            }
        }
        else if (mn == "MUL" || mn == "DIV" || mn == "MOD" ||
                 mn == "AND" || mn == "OR"  || mn == "XOR" ||
                 mn == "SHL" || mn == "SHR") {
            NEED_OPS(2);
            int rd = GET_REG(0), rs = GET_REG(1);
            if (rd < 0 || rs < 0) { res = {false, mn + ": both operands must be registers", li.srcLine}; return res; }
            uint8_t opc;
            if      (mn == "MUL") opc = OP_MUL_RR;
            else if (mn == "DIV") opc = OP_DIV_RR;
            else if (mn == "MOD") opc = OP_MOD_RR;
            else if (mn == "AND") opc = OP_AND_RR;
            else if (mn == "OR")  opc = OP_OR_RR;
            else if (mn == "XOR") opc = OP_XOR_RR;
            else if (mn == "SHL") opc = OP_SHL;
            else                  opc = OP_SHR;
            emitByte(addr, opc); emitByte(addr, (uint8_t)rd); emitByte(addr, (uint8_t)rs);
        }
        else if (mn == "NOT" || mn == "INC" || mn == "DEC" || mn == "PUSH" || mn == "POP") {
            NEED_OPS(1);
            int r = GET_REG(0);
            if (r < 0) { res = {false, mn + ": operand must be register", li.srcLine}; return res; }
            uint8_t opc;
            if      (mn == "NOT")  opc = OP_NOT_R;
            else if (mn == "INC")  opc = OP_INC;
            else if (mn == "DEC")  opc = OP_DEC;
            else if (mn == "PUSH") opc = OP_PUSH;
            else                   opc = OP_POP;
            emitByte(addr, opc); emitByte(addr, (uint8_t)r);
        }
        else if (mn == "JMP" || mn == "JZ" || mn == "JNZ" ||
                 mn == "JG"  || mn == "JL" || mn == "JGE" ||
                 mn == "JLE" || mn == "CALL") {
            NEED_OPS(1);
            uint32_t target;
            if (!resolveAddr(ops[0], target, li.srcLine)) return res;
            uint8_t opc;
            if      (mn == "JMP")  opc = OP_JMP;
            else if (mn == "JZ")   opc = OP_JZ;
            else if (mn == "JNZ")  opc = OP_JNZ;
            else if (mn == "JG")   opc = OP_JG;
            else if (mn == "JL")   opc = OP_JL;
            else if (mn == "JGE")  opc = OP_JGE;
            else if (mn == "JLE")  opc = OP_JLE;
            else                   opc = OP_CALL;
            emitByte(addr, opc); emit32(addr, target);
        }
        else {
            res.success = false;
            res.error = "Unknown instruction: " + mn;
            res.errorLine = li.srcLine;
            return res;
        }

        #undef NEED_OPS
        #undef GET_REG
    }

    return res;
}
