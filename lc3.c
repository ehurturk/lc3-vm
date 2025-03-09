#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

void* read_img(const char* addr) {
    // TODO: Implement VM image read
    return NULL;
}

U16 mem_read(U16 addr) { return 1; }

void handle_args(int argc, const char** argv) {
    if (argc < 2) {
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    for (int i = 0; i < argc; i++) {
        if (!read_img(argv[i])) {
            printf("Failed to load image: %s\n", argv[i]);
            exit(1);
        }
    }
}

int main(int argc, const char** argv) {
    /* Load arguments */
    handle_args(argc, argv);
    /* Setup */

    reg[R_COND] = FL_ZRO;

#define PC_START 0x3000
    reg[R_PC] = PC_START;

    int running = 1;

    while (running) {
        /* fetch the instruction */
        U16 instr = mem_read(reg[R_PC]++);
        U16 op = instr >> 12; /* get the first 4 bytes of the instruction (opcode) */

        switch (op) {
            case OP_ADD:
                break;
            case OP_AND:
                break;
            case OP_BR:
                break;
            case OP_LD:
                break;
            case OP_JMP:
                break;
            case OP_JSR:
                break;
            case OP_LDI:
                break;
            case OP_LDR:
                break;
            case OP_LEA:
                break;
            case OP_NOT:
                break;
            case OP_ST:
                break;
            case OP_STI:
                break;
            case OP_STR:
                break;
            case OP_TRAP:
                break;
            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }

    /* shutdown */
}
