#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define U8 uint8_t
#define U16 uint16_t
#define U32 uint32_t
#define U64 uint64_t
#define I8 int8_t
#define I16 int16_t
#define I32 int32_t
#define I64 int64_t

/*
 * LC-3 architecture has 65536 memory locations
 * Which is 2^16, each storing 16-bit value.
 */
#define MEMORY_MAX (1 << 16)
U16 memory[MEMORY_MAX];

enum {
    // general purpose registers
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    // program counter register:
    R_PC,
    // condition flags register:
    R_COND,
    R_COUNT
};

U16 reg[R_COUNT];

/* Every instruction is 16-bit */
enum {
    OP_BR = 0, /* branch */
    OP_ADD,    /* add */
    OP_LD,     /* load (memory) */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

enum {
    FL_POS = 1 << 0, /* positive */
    FL_ZRO = 1 << 1, /* zero */
    FL_NEG = 1 << 2, /* negative */
};

enum {
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

/* memory mapped registers */
enum {
    MR_KBSR = 0xfe00, /* keyboard status register address */
    MR_KBDR = 0xfe02, /* keyboard data register address */
    MR_DSR = 0xfe04,  /* display status register addrress */
    MR_DDR = 0xfe06   /* display data register address */
};

struct termios original_tio;

void disable_input_buffering() {
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() { tcsetattr(STDIN_FILENO, TCSANOW, &original_tio); }

uint16_t check_key() {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

void handle_interrupt(int signal) {
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

/*
 *     Example LC3 assembly program
 *     .ORIG x3000                       ; address in memory where the program will be
 *     loaded
 *     LEA R0, HELLO_STR                 ; load the address of HELLO_STR string into R0
 *     PUTs                              ; output the string pointed to R0 to the console
 *     HALT                              ; halt the program
 *     HELLO_STR .STRINGZ "Hello World!" ; store the string here
 *     .END                              ; mark the end
 *
 */

/*
 * Procedure:
 *  1. Load one instruction from memory at the address of the PC register
 *  2. Increment PC
 *  3. Look at the opcode -> determine the instruction
 *  4. Perform the instruction using parameters
 *  5. Go back to step1
 */

U16 swap16(U16 x) { return (x << 8) | (x >> 8); }

/* read LC-3 program file */
void read_image_file(FILE* file) {
    U16 origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    U16 max_read = MEMORY_MAX - origin;
    U16* p = memory + origin;

    size_t read = fread(p, sizeof(U16), max_read, file);

    /* swap to little endian - move lower bits to lower addresses */
    while (read-- > 0) {
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return 0;
    }
    read_image_file(file);
    fclose(file);
    return 1;
}

U16 mem_read(U16 addr) {
    if (addr == MR_KBSR) {
        if (check_key()) {
            memory[MR_KBSR] = (1 << 15); /* the ready bit KBSR[15] is set to one */
            memory[MR_KBDR] = getchar(); /* set keyboard data to bits [7:0] */
        } else {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[addr];
}

void mem_write(U16 addr, U16 data) { memory[addr] = data; }

void handle_args(int argc, const char** argv) {
    if (argc < 2) {
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    for (int i = 0; i < argc; i++) {
        if (!read_image(argv[i])) {
            printf("Failed to load image: %s\n", argv[i]);
            exit(1);
        }
    }
}

U16 sign_extend(U16 x, int bit_count) {
    if (x >> (bit_count - 1) & 1) {
        x |= 0xffff << bit_count;
    }
    return x;
}

void update_cflags(U16 r) {
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) {
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

int main(int argc, const char** argv) {
    /* Load arguments */
    handle_args(argc, argv);
    /* Setup */

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    reg[R_COND] = FL_ZRO;

#define PC_START 0x3000
    reg[R_PC] = PC_START;

    int running = 1;

    while (running) {
        /* fetch the instruction */
        U16 instr = mem_read(reg[R_PC]++);
        // printf("%d\n", instr);
        U16 op = instr >> 12; /* get the first 4 bytes of the instruction (opcode) */

        switch (op) {
            case OP_ADD: {
                U16 imm_mode = (0x1 & (instr >> 5));
                U16 dest_reg = (0x7 & (instr >> 9));
                U16 s1_reg = (0x7 & (instr >> 6));
                U16 s2 = 0;
                if (imm_mode) {
                    s2 = sign_extend((0x1f & instr), 5);
                } else {
                    s2 = instr & 0x7;
                }
                reg[dest_reg] = reg[s1_reg] + s2;
                update_cflags(dest_reg);
                break;
            }

            case OP_AND: {
                U16 imm_mode = (0x1 & (instr >> 5));
                U16 dest_reg = (0x7 & (instr >> 9));
                U16 s1_reg = (0x7 & (instr >> 6));
                U16 s2 = 0;
                if (imm_mode) {
                    s2 = sign_extend((0x1f & instr), 5);
                } else {
                    s2 = instr & 0x7;
                }
                reg[dest_reg] = reg[s1_reg] & s2;
                update_cflags(dest_reg);
                break;
            }
            case OP_BR: {
                U16 p = (0x7 & (instr >> 9));
                if (p & reg[R_COND]) {
                    reg[R_PC] += sign_extend(instr & 0x1ff, 9);
                }
                break;
            }
            t case OP_JMP: {
                U16 breg = (instr >> 6) & 0x7;
                reg[R_PC] = reg[breg];
                break;
            }

            case OP_JSR: {
                reg[R_R7] = reg[R_PC];
                if (((instr >> 11) & 0x1) == 0) {
                    reg[R_PC] = reg[(instr >> 6) & 0x7];
                } else {
                    reg[R_PC] += sign_extend(instr & 0x7ff, 11);
                }
                break;
            }

            case OP_LD: {
                U16 dr = (instr >> 9) & 0x7;
                reg[dr] = mem_read(reg[R_PC] + sign_extend(instr & 0x1ff, 9));
                update_cflags(dr);
                break;
            }

            case OP_LDI: {
                U16 dr = (instr >> 9) & 0x7;
                reg[dr] = mem_read(mem_read(reg[R_PC] + sign_extend(instr & 0x1ff, 9)));
                update_cflags(dr);
                break;
            }

            case OP_LDR: {
                U16 dr = (instr >> 9) & 0x7;
                U16 br = (instr >> 6) & 0x7;
                reg[dr] = mem_read(reg[br] + sign_extend(instr & 0x3f, 6));
                update_cflags(dr);
                break;
            }

            case OP_LEA: {
                U16 dest = (instr >> 9) & 0x7;
                U16 pcoffset = instr & 0x1ff;
                reg[dest] = reg[R_PC] + sign_extend(pcoffset, 9);
                update_cflags(dest);
                break;
            }

            case OP_NOT: {
                U16 dest = (instr >> 9) & 0x7;
                U16 src = (instr >> 6) & 0x7;
                reg[dest] = ~reg[src];
                update_cflags(dest);
                break;
            }

            case OP_ST: {
                U16 sr = (instr >> 9) & 0x7;
                U16 pcoffset = instr & 0x1ff;
                mem_write(reg[R_PC] + sign_extend(pcoffset, 9), reg[sr]);
                break;
            }

            case OP_STI: {
                U16 sr = (instr >> 9) & 0x7;
                U16 pcoffset = instr & 0x1ff;
                mem_write(mem_read(reg[R_PC] + sign_extend(pcoffset, 9)), reg[sr]);
                break;
            }

            case OP_STR: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                mem_write(reg[r1] + offset, reg[r0]);

                break;
            }

            case OP_TRAP: {
                /* save the pc in r7 as a return address */
                reg[R_R7] = reg[R_PC];

                switch (instr & 0xFF) {
                    case TRAP_GETC: {
                        reg[R_R0] = (U16)getchar();
                        update_cflags(R_R0);
                        break;
                    }
                    case TRAP_OUT: {
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);
                        break;
                    }
                    case TRAP_PUTS: {
                        U16* c = memory + reg[R_R0];
                        while (*c) {
                            putc((char)*c, stdout);
                            ++c;
                        }
                        fflush(stdout);
                        break;
                    }
                    case TRAP_IN: {
                        printf("Enter a character: ");
                        char c = getchar();
                        putc(c, stdout);
                        fflush(stdout);
                        reg[R_R0] = (U16)c;
                        update_cflags(R_R0);
                        break;
                    }
                    case TRAP_PUTSP: {
                        uint16_t* c = memory + reg[R_R0];
                        while (*c) {
                            char char1 = (*c) & 0xFF;
                            putc(char1, stdout);
                            char char2 = (*c) >> 8;
                            if (char2) putc(char2, stdout);
                            ++c;
                        }
                        fflush(stdout);
                        break;
                    }
                    case TRAP_HALT: {
                        puts("HALT");
                        fflush(stdout);
                        running = 0;
                        break;
                    }
                }
            }

            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }

    /* shutdown */
    restore_input_buffering();
}
