
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdlib.h>
#include <ioports.h>

//#define DISASM
//#define THROTTLE_LOG

#define disasm_log  write_log("[disasm] %16d %04X ", total_cycles, cpu.pc); write_log

#define REG_A       7
#define REG_B       0
#define REG_C       1
#define REG_D       2
#define REG_E       3
#define REG_H       4
#define REG_L       5

#define REG_BC      0
#define REG_DE      1
#define REG_HL      2
#define REG_SP      3

#define THROTTLE_THRESHOLD  2.0    // ms

const char *registers[] = {
    "b", "c", "d", "e", "h", "l", "UNDEFINED", "a"
};

const char *registers16[] = {
    "bc", "de", "hl", "sp"
};

cpu_t cpu;
double cycles_time = 0.0;
int cycles = 0;
int total_cycles = 0;
void (*opcodes[256])();
int cpu_speed;

void count_cycles(int n) {
    n++;    // all cpu cycles are practically always one cycle longer
    total_cycles += n;
    cycles += n;

    timing.current_cycles += n;

    double msec = (double)(n * 1000.0 / cpu_speed);

    cycles_time += msec;
    if(cycles_time >= THROTTLE_THRESHOLD) {
#ifdef THROTTLE_LOG
        write_log("[cpu] accumulated %d cycles, delaying %d ms\n", cycles, (int)cycles_time);
#endif
        int msec_int = (int)cycles_time;
        SDL_Delay(msec_int);
        cycles_time = 0.0;
        cycles = 0;
    }
}

void cpu_log() {
    write_log(" AF = 0x%04X   BC = 0x%04X   DE = 0x%04X\n", cpu.af, cpu.bc, cpu.de);
    write_log(" HL = 0x%04X   SP = 0x%04X   PC = 0x%04X\n", cpu.hl, cpu.sp, cpu.pc);
    write_log(" executed total cycles = %d\n", total_cycles);
    write_log(" time until next CPU throttle = %lf ms\n", THROTTLE_THRESHOLD - cycles_time);
}

void dump_cpu() {
    cpu_log();
    die(-1, NULL);
}

void cpu_start() {
    // initial cpu state
    cpu.af = 0x01B0;
    cpu.bc = 0x0013;
    cpu.de = 0x00D8;
    cpu.hl = 0x014D;
    cpu.sp = 0xFFFE;
    cpu.pc = 0x0100;    // skip the fixed rom and just exec the cartridge

    io_if = 0;
    io_ie = 0;

    if(is_cgb) {
        cpu_speed = CGB_CPU_SPEED;
    } else {
        cpu_speed = GB_CPU_SPEED;
    }

    write_log("[cpu] started with speed %lf MHz\n", (double)cpu_speed/1000000);

    // determine values that will be used to keep track of timing
    timing.current_cycles = 0;
    timing.cpu_cycles_ms = cpu_speed / 1000;
    timing.cpu_cycles_vline = (int)((double)timing.cpu_cycles_ms * REFRESH_TIME_LINE);

    write_log("[cpu] cycles per ms = %d\n", timing.cpu_cycles_ms);
    write_log("[cpu] cycles per v-line refresh = %d\n", timing.cpu_cycles_vline);
}

void cpu_cycle() {
    uint8_t opcode = read_byte(cpu.pc);

    if(!opcodes[opcode]) {
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", opcode, read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        dump_cpu();
    } else {
        opcodes[opcode]();
    }
}

void write_reg8(int reg, uint8_t r) {
    switch(reg) {
    case REG_A:
        cpu.af &= 0x00FF;
        cpu.af |= (uint16_t)r << 8;
        break;
    case REG_B:
        cpu.bc &= 0x00FF;
        cpu.bc |= (uint16_t)r << 8;
        break;
    case REG_C:
        cpu.bc &= 0xFF00;
        cpu.bc |= (uint16_t)r;
        break;
    case REG_D:
        cpu.de &= 0x00FF;
        cpu.de |= (uint16_t)r << 8;
        break;
    case REG_E:
        cpu.de &= 0xFF00;
        cpu.de |= (uint16_t)r;
        break;
    case REG_H:
        cpu.hl &= 0x00FF;
        cpu.hl |= (uint16_t)r << 8;
        break;
    case REG_L:
        cpu.hl &= 0xFF00;
        cpu.hl |= (uint16_t)r;
        break;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        return dump_cpu();
    }
}

uint8_t read_reg8(int reg) {
    uint8_t ret;
    switch(reg) {
    case REG_A:
        ret = cpu.af >> 8;
        break;
    case REG_B:
        ret = cpu.bc >> 8;
        break;
    case REG_C:
        ret = cpu.bc & 0xFF;
        break;
    case REG_D:
        ret = cpu.de >> 8;
        break;
    case REG_E:
        ret = cpu.de & 0xFF;
        break;
    case REG_H:
        ret = cpu.hl >> 8;
        break;
    case REG_L:
        ret = cpu.hl & 0xFF;
        break;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        dump_cpu();
        return 0xFF;    // unreachable anyway
    }

    return ret;
}

void write_reg16(int reg, uint16_t r) {
    switch(reg) {
    case REG_BC:
        cpu.bc = r;
        break;
    case REG_DE:
        cpu.de = r;
        break;
    case REG_HL:
        cpu.hl = r;
        break;
    case REG_SP:
        cpu.sp = r;
        break;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        return dump_cpu();
    }
}

uint16_t read_reg16(int reg) {
    switch(reg) {
    case REG_BC:
        return cpu.bc;
    case REG_DE:
        return cpu.de;
    case REG_HL:
        return cpu.hl;
    case REG_SP:
        return cpu.sp;
    default:
        write_log("undefined opcode %02X %02X %02X, dumping CPU state...\n", read_byte(cpu.pc), read_byte(cpu.pc+1), read_byte(cpu.pc+2));
        dump_cpu();
        return 0xFFFF;    // unreachable
    }
}

/*
   INDIVIDUAL INSTRUCTIONS ARE IMPLEMENTED HERE
 */

void nop() {
#ifdef DISASM
    disasm_log("nop\n");
#endif

    cpu.pc++;
    count_cycles(1);
}

void jp_nn() {
    uint16_t new_pc = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("jp 0x%04X\n", new_pc);
#endif

    cpu.pc = new_pc;
    count_cycles(4);
}

void ld_r_r() {
    // 0b01xxxyyy
    uint8_t opcode = read_byte(cpu.pc);
    int x = (opcode >> 3) & 7;
    int y = opcode & 7;

#ifdef DISASM
    disasm_log("ld %s, %s\n", registers[x], registers[y]);
#endif

    uint8_t src = read_reg8(y);
    write_reg8(x, src);

    cpu.pc++;
    count_cycles(1);
}

void sbc_a_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("sbc a, %s\n", registers[reg]);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r;
    r = read_reg8(reg);

    a -= r;
    if(cpu.af & FLAG_CY) a--;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(1);
}

void sub_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("sub %s\n", registers[reg]);
#endif

    uint8_t a = read_reg8(REG_A);
    uint8_t r;
    r = read_reg8(reg);

    a -= r;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(1);
}

void dec_r() {
    uint8_t opcode = read_byte(cpu.pc);

    int reg = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("dec %s\n", registers[reg]);
#endif

    uint8_t r;
    r = read_reg8(reg);

    uint8_t old = r;
    r--;

    cpu.af |= FLAG_N;

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((r & 0x0F) > (old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(reg, r);

    cpu.pc++;
    count_cycles(1);
}

void ld_r_xx() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 3) & 7;
    uint8_t val = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld %s, 0x%02X\n", registers[reg], val);
#endif

    write_reg8(reg, val);

    cpu.pc += 2;
    count_cycles(2);
}

void inc_r() {
    uint8_t opcode = read_byte(cpu.pc);

    int reg = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("inc %s\n", registers[reg]);
#endif

    uint8_t r;
    r = read_reg8(reg);

    uint8_t old = r;
    r++;

    cpu.af &= (~FLAG_N);

    if(!r) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if((r & 0x0F) < (old & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    write_reg8(reg, r);

    cpu.pc++;
    count_cycles(1);
}

void jr_e() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;
        cpu.pc -= pe;
    } else {
        #ifdef DISASM
            disasm_log("jr 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;
        cpu.pc += e;
    }

    count_cycles(3);
}

void ld_r_hl() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 3) & 7;

#ifdef DISASM
    disasm_log("ld %s, (hl)\n", registers[reg]);
#endif

    uint8_t val = read_byte(cpu.hl);

    write_reg8(reg, val);

    cpu.pc++;
    count_cycles(2);
}

void ld_r_xxxx() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;
    uint16_t val = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld %s, 0x%04X\n", registers16[reg], val);
#endif

    write_reg16(reg, val);

    cpu.pc += 3;
    count_cycles(3);
}

void cpl() {
#ifdef DISASM
    disasm_log("cpl\n");
#endif

    write_reg8(REG_A, read_reg8(REG_A) ^ 0xFF);

    cpu.af |= FLAG_N | FLAG_H;

    cpu.pc++;
    count_cycles(1);
}

void ld_bc_a() {
#ifdef DISASM
    disasm_log("ld (bc), a\n");
#endif

    uint8_t a = read_reg8(REG_A);
    write_byte(cpu.bc, a);

    cpu.pc++;
    count_cycles(2);
}

void inc_r16() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = (opcode >> 4) & 3;

#ifdef DISASM
    disasm_log("inc %s\n", registers16[reg]);
#endif

    uint16_t val = read_reg16(reg);
    val++;
    write_reg16(reg, val);

    cpu.pc++;
    count_cycles(2);
}

void xor_r() {
    uint8_t opcode = read_byte(cpu.pc);
    int reg = opcode & 7;

#ifdef DISASM
    disasm_log("xor %s\n", registers[reg]);
#endif

    uint8_t val = read_reg8(reg);
    uint8_t a = read_reg8(REG_A);

    a ^= val;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    cpu.af &= ~(FLAG_N | FLAG_H | FLAG_CY);

    write_reg8(REG_A, a);

    cpu.pc++;
    count_cycles(1);
}

void ldd_hl_a() {
#ifdef DISASM
    disasm_log("ldd (hl), a\n");
#endif

    uint8_t a = read_reg8(REG_A);

    write_byte(cpu.hl, a);
    cpu.hl--;

    cpu.pc++;
    count_cycles(2);
}

void jr_nz() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr nz 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;

        if(cpu.af & FLAG_ZF) {
            // ZF is set; condition false
            count_cycles(2);
        } else {
            // ZF not set; condition true
            cpu.pc -= pe;
            count_cycles(3);
        }
    } else {
        #ifdef DISASM
            disasm_log("jr nz 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;

        if(cpu.af & FLAG_ZF) {
            // ZF is set; condition false
            count_cycles(2);
        } else {
            // ZF not set; condition true
            cpu.pc += e;
            count_cycles(3);
        }
    }
}

void di() {
#ifdef DISASM
    disasm_log("di\n");
#endif

    cpu.ime = 0;
    cpu.pc++;
    count_cycles(1);
}

void ldh_a8_a() {
    uint8_t a8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("ldh (0x%02X), a\n", a8);
#endif

    uint16_t addr = 0xFF00 + a8;
    write_byte(addr, read_reg8(REG_A));

    cpu.pc += 2;
    count_cycles(3);
}

void cp_xx() {
    uint8_t val = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("cp 0x%02X\n", val);
#endif

    uint8_t a = read_reg8(REG_A);

    a -= val;

    cpu.af |= FLAG_N;

    if(!a) cpu.af |= FLAG_ZF;
    else cpu.af &= (~FLAG_ZF);

    if(a > read_reg8(REG_A)) cpu.af |= FLAG_CY;
    else cpu.af &= (~FLAG_CY);

    if((a & 0x0F) < (read_reg8(REG_A) & 0x0F)) cpu.af |= FLAG_H;
    else cpu.af &= (~FLAG_H);

    cpu.pc += 2;
    count_cycles(2);
}

void jr_z() {
    uint8_t e = read_byte(cpu.pc+1);

    if(e & 0x80) {
        uint8_t pe = ~e;
        pe++;
        #ifdef DISASM
            disasm_log("jr z 0x%02X (-%d) (0x%04X)\n", e, pe, (cpu.pc - pe) + 2);
        #endif

        cpu.pc += 2;

        if(!(cpu.af & FLAG_ZF)) {
            // ZF is false; condition false
            count_cycles(2);
        } else {
            // ZF is set; condition true
            cpu.pc -= pe;
            count_cycles(3);
        }
    } else {
        #ifdef DISASM
            disasm_log("jr z 0x%02X (+%d) (0x%04X)\n", e, e, cpu.pc + 2 + e);
        #endif

        cpu.pc += 2;

        if(!(cpu.af & FLAG_ZF)) {
            // ZF is false; condition false
            count_cycles(2);
        } else {
            // ZF is set; condition true
            cpu.pc += e;
            count_cycles(3);
        }
    }
}

void ld_a16_a() {
    uint16_t addr = read_word(cpu.pc+1);

#ifdef DISASM
    disasm_log("ld (0x%04X), a\n", addr);
#endif

    write_byte(addr, read_reg8(REG_A));
    cpu.pc += 3;
    count_cycles(4);
}

void ldh_a_a8() {
    uint8_t a8 = read_byte(cpu.pc+1);

#ifdef DISASM
    disasm_log("ldh a, (0x%02X)\n", a8);
#endif

    uint16_t addr = 0xFF00 + a8;
    write_reg8(REG_A, read_byte(addr));

    cpu.pc += 2;
    count_cycles(3);
}

// lookup table
void (*opcodes[256])() = {
    nop, ld_r_xxxx, ld_bc_a, inc_r16, NULL, dec_r, ld_r_xx, NULL,  // 0x00
    NULL, NULL, NULL, NULL, NULL, dec_r, ld_r_xx, NULL,  // 0x08
    NULL, ld_r_xxxx, NULL, inc_r16, NULL, dec_r, ld_r_xx, NULL,  // 0x10
    jr_e, NULL, NULL, NULL, NULL, dec_r, ld_r_xx, NULL,  // 0x18
    jr_nz, ld_r_xxxx, NULL, inc_r16, NULL, dec_r, ld_r_xx, NULL,  // 0x20
    jr_z, NULL, NULL, NULL, inc_r, dec_r, ld_r_xx, cpl,  // 0x28
    NULL, ld_r_xxxx, ldd_hl_a, inc_r16, NULL, NULL, NULL, NULL,  // 0x30
    NULL, NULL, NULL, NULL, NULL, dec_r, ld_r_xx, NULL,  // 0x38

    // 8-bit loads
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x40
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x48
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x50
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x58
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x60
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x68
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0x70
    ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_r, ld_r_hl, ld_r_r,  // 0x78

    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0x80
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0x88
    sub_r, sub_r, sub_r, sub_r, sub_r, sub_r, NULL, sub_r,  // 0x90
    sbc_a_r, sbc_a_r, sbc_a_r, sbc_a_r, sbc_a_r, sbc_a_r, NULL, sbc_a_r,  // 0x98
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0xA0
    xor_r, xor_r, xor_r, xor_r, xor_r, xor_r, NULL, xor_r,  // 0xA8
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0xB0
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0xB8
    NULL, NULL, NULL, jp_nn, NULL, NULL, NULL, NULL,  // 0xC0
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0xC8
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0xD0
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0xD8
    ldh_a8_a, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  // 0xE0
    NULL, NULL, ld_a16_a, NULL, NULL, NULL, NULL, NULL,  // 0xE8
    ldh_a_a8, NULL, NULL, di, NULL, NULL, NULL, NULL,  // 0xF0
    NULL, NULL, NULL, NULL, NULL, NULL, cp_xx, NULL,  // 0xF8
};
