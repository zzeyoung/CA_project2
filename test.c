#include <stdio.h>
#include <stdlib.h>

#define MEM_SIZE 10000*10000
#define INPUTFILENAME "simple3.bin"

unsigned int memory[MEM_SIZE];
unsigned int reg[32];
unsigned int pc = 0;
unsigned int instruction_count = 0;
unsigned int instruction, opcode, rs, rt, rd, shamt, funct, immediate, address;
unsigned int aluResult = 0;

int regDest, ALUSrc, memToReg, regWrite, memRead, memWrite, branch, jump, ALUOp;
unsigned int i_type_count = 0;
unsigned int r_type_count = 0;
unsigned int sw_count = 0;
unsigned int lw_count = 0;
unsigned int taken_branch_count = 0;
unsigned int not_taken_branch_count = 0;
unsigned int jump_count = 0;

void fetch();
void decode();
void execute();
void memory_access();
void write_back();
void load_program(const char* filename);
void initialize();
unsigned int sign_extend(unsigned int value, int bits);

int main() {
    initialize();
    load_program(INPUTFILENAME);

    while (pc != 0xFFFFFFFF) {
        printf("\n------------cycle:%d--------------\n", instruction_count + 1);
        fetch();
        decode();
        execute();
        memory_access();
        write_back();
        instruction_count++;
    }
    printf("\n------------end---------------\n\n");

    printf("Final value in v0 (r2): %u\n", reg[2]);

    printf("Total instruction: %u\n", instruction_count);
    printf("Execution cycles: %u\n", instruction_count);
    printf("I-type ALU op: %u\n", i_type_count);
    printf("R-type ALU op: %u\n", r_type_count);
    printf("SW: %u\n", sw_count);
    printf("LW: %u\n", lw_count);
    printf("Taken branch: %u\n", taken_branch_count);
    printf("Not-taken branch: %u\n", not_taken_branch_count);
    printf("Jump: %u\n", jump_count);

    return 0;
}

void initialize() {
    for (int i = 0; i < MEM_SIZE; i++) {
        memory[i] = 0;
    }
    for (int i = 0; i < 32; i++) {
        reg[i] = 0;
    }
    reg[29] = 0x80000;
    reg[31] = 0xFFFFFFFF;
    pc = 0;
}

int reverseBytes(int value) {
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
        ((value & 0xFF0000) >> 8) | ((value >> 24) & 0xFF);
}

void load_program(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    int i = 0;
    while (i < MEM_SIZE && fread(&memory[i], sizeof(unsigned int), 1, file) == 1) {
        memory[i] = reverseBytes(memory[i]);
        i++;
    }

    if (ferror(file)) {
        perror("Failed to read the file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    printf("Loaded %d instructions into memory.\n", i);
}

void fetch() {
    instruction = memory[pc / 4];
    printf("instruction : %x\n", instruction);
    pc += 4;
}

void decode() {
    opcode = (instruction >> 26) & 0x0000003F;
    rs = (instruction >> 21) & 0x1F;
    rt = (instruction >> 16) & 0x1F;
    rd = (instruction >> 11) & 0x0000001F;
    shamt = (instruction >> 6) & 0x0000001F;
    funct = instruction & 0x0000003F;
    immediate = instruction & 0x0000FFFF;
    address = instruction & 0x03FFFFFF;

    regDest = ALUSrc = memToReg = regWrite = memRead = memWrite = branch = jump = ALUOp = 0;

    switch (opcode) {
    case 0x00:
        ALUOp = 1;
        regDest = 1;
        regWrite = 1;
        r_type_count++;
        switch (funct) {
        case 0x20:
            ALUOp = 2;
            break;
        case 0x21:
            ALUOp = 2;
            break;
        case 0x22:
            ALUOp = 6;
            break;
        case 0x24:
            ALUOp = 0;
            break;
        case 0x25:
            ALUOp = 1;
            break;
        case 0x2A:
            ALUOp = 7;
            break;
        case 0x00:
            ALUOp = 3;
            break;
        case 0x02:
            ALUOp = 4;
            break;
        case 0x08:
            jump = 1;
            regWrite = 0;
            pc = reg[rs];
            jump_count++;
            break;
        default:
            printf("Unknown R-type function\n");
        }
        break;
    case 0x08:
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 2;
        i_type_count++;
        break;
    case 0x09:
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 2;
        i_type_count++;
        break;
    case 0x0C:
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 0;
        i_type_count++;
        break;
    case 0x0D:
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 1;
        i_type_count++;
        break;
    case 0x0E:
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 5;
        i_type_count++;
        break;
    case 0x0A:
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 7;
        printf("SLTI 실행함\n");
        i_type_count++;
        break;
    case 0x23:
        ALUSrc = 1;
        memRead = 1;
        memToReg = 1;
        regWrite = 1;
        ALUOp = 2;
        lw_count++;
        r_type_count++;
        break;
    case 0x2B:
        ALUSrc = 1;
        memWrite = 1;
        ALUOp = 2;
        sw_count++;
        r_type_count++;
        break;
    case 0x04:
        branch = 1;
        if (reg[rs] == reg[rt]) {
            pc += sign_extend(immediate, 16) << 2;
            taken_branch_count++;
        }
        else {
            not_taken_branch_count++;
        }
        break;
    case 0x05:
        branch = 1;
        if (reg[rs] != reg[rt]) {
            pc += sign_extend(immediate, 16) << 2;
            taken_branch_count++;
        }
        else {
            not_taken_branch_count++;
        }
        break;
    case 0x02:
        jump = 1;
        pc = (pc & 0xF0000000) | (address << 2);
        jump_count++;
        break;
    default:
        printf("Unknown opcode\n");
    }
    printf("pc[0x%x]\n", pc);
}

void execute() {
    aluResult = 0;
    unsigned int src1 = reg[rs];
    unsigned int src2 = ALUSrc ? sign_extend(immediate, 16) : reg[rt];
    switch (ALUOp) {
    case 0:
        aluResult = src1 & src2;
        break;
    case 1:
        aluResult = src1 | src2;
        break;
    case 2:
        aluResult = src1 + src2;
        break;
    case 3:
        aluResult = src2 << shamt;
        break;
    case 4:
        aluResult = src2 >> shamt;
        break;
    case 5:
        aluResult = src1 ^ src2;
        break;
    case 6:
        aluResult = src1 - src2;
        break;
    case 7:
        aluResult = src1 < src2 ? 1 : 0;
        break;
    default:
        printf("Unknown ALU operation\n");
    }
    if (!branch && !jump) {
        if (memRead || memWrite) {
            address = aluResult;
        }
    }
    printf("ALU operation result: %u\n", aluResult);
}

void memory_access() {
    if (memRead == 1) {
        if (address / 4 >= MEM_SIZE) {
            fprintf(stderr, "Memory read error: address out of bounds.\n");
            exit(EXIT_FAILURE);
        }
        reg[rt] = memory[address / 4];
        printf("register[%d] = memory[%x]", rt, address);
    }
    else if (memWrite == 1) {
        if (address / 4 >= MEM_SIZE) {
            fprintf(stderr, "Memory write error: address out of bounds.\n");
            exit(EXIT_FAILURE);
        }
        memory[address / 4] = reg[rt];
        printf("memory[%x] = reg[%d]\n", address, rt);
    }
    printf("Memory access at address %x, value = %x\n", address, memory[address / 4]);
}

void write_back() {
    if (regWrite && !memRead) {
        if (rt >= 32) {
            printf("%x", rt);
            fprintf(stderr, "Register index out of bounds.\n");
            exit(EXIT_FAILURE);
        }
        if (memToReg) {
            if (address / 4 >= MEM_SIZE) {
                fprintf(stderr, "Memory read error for write-back: address out of bounds.\n");
                exit(EXIT_FAILURE);
            }
            reg[rt] = memory[address / 4];
        }
        else {
            if (regDest) {
                reg[rd] = aluResult;
                printf("reg[%d] = %x\n", rd, aluResult);
            }
            else {
                reg[rt] = aluResult;
                printf("reg[%d] = %x\n", rt, aluResult);
            }
        }
    }
    printf("Before write-back, reg[%d] = %u\n", rt, reg[rt]);
    printf("After write-back, reg[%d] = %u\n", rt, reg[rt]);
}

unsigned int sign_extend(unsigned int value, int bits) {
    int shift = 32 - bits;
    int extended = (int)(short)(value << (16 - bits)) >> (16 - bits);
    return (unsigned int)extended;
}
