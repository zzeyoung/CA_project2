#include <stdio.h>
#include <stdlib.h>

#define MEM_SIZE 1024*1024
#define INPUTFILENAME "simple3.bin"

unsigned int memory[MEM_SIZE];  // 메모리 배열
unsigned int reg[32];  // 레지스터 배열
unsigned int pc = 0;  // 프로그램 카운터
unsigned int instruction_count = 0;  // 실행된 명령어의 수
unsigned int instruction, opcode, rs, rt, rd, shamt, funct, immediate, address;
unsigned int aluResult = 0;

// 제어 신호
int regDest, ALUSrc, memToReg, regWrite, memRead, memWrite, branch, jump, ALUOp;

// 명령어 통계
unsigned int i_type_count = 0;
unsigned int r_type_count = 0;
unsigned int sw_count = 0;
unsigned int lw_count = 0;
unsigned int taken_branch_count = 0;
unsigned int not_taken_branch_count = 0;
unsigned int jump_count = 0;

// 함수 선언
void fetch();
void decode();
void execute();
void memory_access();
void write_back();
void load_program(const char* filename);
void initialize();
unsigned int sign_extend(unsigned int value, int bits);

int main() {

    initialize();  // 시뮬레이터 초기화
    load_program(INPUTFILENAME);  // 프로그램 로딩

    while (pc != 0xFFFFFFFF) {  // 무한 루프
        printf("\n------------cycle:%d--------------\n", instruction_count + 1);
        fetch();  // 명령어 가져오기
        decode();  // 명령어 해석
        execute();  // 명령어 실행
        memory_access();  // 메모리 접근
        write_back();  // 결과 레지스터에 쓰기
        instruction_count++;  // 실행된 명령어 수 증가
    }
    printf("\n------------end---------------\n\n");

    printf("Final value in v0 (r2): %u\n", reg[2]);  // 최종 결과 값 출력

    // 최종 통계 출력
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
    FILE* file = fopen(filename, "rb");  // 파일을 바이너리 모드로 열기
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);  // 파일 열기 실패 시 프로그램 종료
    }

    // 파일에서 메모리 배열로 데이터 읽기, 4바이트 단위로
    int i = 0;
    while (i < MEM_SIZE && fread(&memory[i], sizeof(unsigned int), 1, file) == 1) {
        memory[i] = reverseBytes(memory[i]); // 바이트 순서 변환
        i++;
    }

    if (ferror(file)) {  // 파일 읽기 에러 확인
        perror("Failed to read the file");
        fclose(file);
        exit(EXIT_FAILURE);  // 파일 읽기 실패 시 프로그램 종료
    }

    fclose(file);  // 파일 닫기
    printf("Loaded %d instructions into memory.\n", i);
}


void fetch() {
    instruction = memory[pc / 4];  // 현재 PC에서 명령어 가져오기
    printf("instruction : %x\n", instruction);

    pc += 4;  // PC를 다음 명령어로 이동
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
    
    // 제어 신호 리셋
    regDest = ALUSrc = memToReg = regWrite = memRead = memWrite = branch = jump = ALUOp = 0;

    switch (opcode) {
    case 0x00:
        ALUOp = 1; // R-type 명령어에 대한 ALU 연산 설정
        regDest = 1; // 목적지 레지스터는 rd
        regWrite = 1; // 레지스터 쓰기 활성화
        r_type_count++;  // R-type 명령어 수 증가
        switch (funct) {
        case 0x20: // ADD
            ALUOp = 2; // 덧셈 연산
            break;
        case 0x21: //ADDU
            ALUOp = 2;
            break;
        case 0x22: // SUB
            ALUOp = 6; // 뺄셈 연산
            break;
        case 0x24: // AND
            ALUOp = 0; // AND 연산
            break;
        case 0x25: // OR
            ALUOp = 1; // OR 연산
            break;
        case 0x2A: // SLT
            ALUOp = 7; // 더 작으면 1, 아니면 0
            break;
        case 0x00: // SLL
            ALUOp = 3; // 왼쪽으로 논리 시프트
            break;
        case 0x02: // SRL
            ALUOp = 4; // 오른쪽으로 논리 시프트
            break;
        case 0x08: // JR
            jump = 1;
            regWrite = 0;
            pc = reg[rs];
            jump_count++;
            break;
        default:
            printf("Unknown R-type function\n");
        }
        break;
     case 0x08: // ADDI 명령어
        ALUSrc = 1; // 두 번째 ALU 피연산자는 즉시값
        regWrite = 1; // 레지스터 쓰기 활성화
        ALUOp = 2; // 덧셈 연산
        i_type_count++;  // I-type 명령어 수 증가
        break;
    case 0x09: // ADDIU
        ALUSrc = 1; //두번째 ALU 피연산자는 즉시값
        regWrite = 1;
        ALUOp = 2; // 덧셈 연산, 오버플로우 무시
        i_type_count++;
        //printf("실행됏어요~~\n");
        break;
    case 0x0C: // ANDI
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 0; // AND 연산
        i_type_count++;
        break;
    case 0x0D: // ORI
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 1; // OR 1연산
        i_type_count++;
        break;
    case 0x0E: // XORI
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 5; // XOR 연산
        i_type_count++;
        break;
    case 0x0A: // SLTI
        ALUSrc = 1;
        regWrite = 1;
        ALUOp = 7; // 즉시값보다 작으면 1, 아니면 0
        printf("SLTI 실행함\n");
        i_type_count++;
        break;
    case 0x23: // LW
        ALUSrc = 1; // 주소 계산은 즉시값 사용
        memRead = 1;
        memToReg = 1;
        regWrite = 1;
        ALUOp = 2;
        lw_count++;
        r_type_count++;
        break;
    case 0x2B: // SW
        ALUSrc = 1; // 주소 계산은 즉시값 사용
        memWrite = 1;
        ALUOp = 2;
        sw_count++;
        r_type_count++;
        break;
    case 0x04: // BEQ
        branch = 1;
        if (reg[rs] == reg[rt]) {
            pc += sign_extend(immediate, 16) << 2; // 분기 성공시 PC 업데이트
            taken_branch_count++;
        }
        else {
            not_taken_branch_count++;
        }
        break;
    case 0x05: // BNE
        branch = 1;
        if (reg[rs] != reg[rt]) {
            pc += sign_extend(immediate, 16) << 2; // 분기 성공시 PC 업데이트
            taken_branch_count++;
        }
        else {
            not_taken_branch_count++;
        }
        break;
    case 0x02: // J
        jump = 1;
        pc = (pc & 0xF0000000) | (address << 2); // 점프 주소 계산
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
    case 0: // AND
        aluResult = src1 & src2;
        break;
    case 1: // OR
        aluResult = src1 | src2;
        break;
    case 2: // ADD
        aluResult = src1 + src2;
        break;
    case 3: // SLL
        aluResult = src2 << shamt;
        break;
    case 4: // SRL
        aluResult = src2 >> shamt;
        break;
    case 5: // XOR
        aluResult = src1 ^ src2;
        break;
    case 6: // SUB
        aluResult = src1 - src2;
        break;
    case 7: // SLT
        aluResult = src1 < src2 ? 1 : 0;
        break;
    default:
        printf("Unknown ALU operation\n");
    }
     if (!branch && !jump) {
        if (memRead || memWrite) {
            address = aluResult; // LW와 SW를 위해 주소 저장
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
    //printf("Memory access at address %x, value = %x\n", address, memory[address / 4]);
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
    int shift = 32 - bits;  // 부호 확장을 위한 쉬프트 계산       int shift = 32 - bits;
    int extended = (int)value << shift;  // 왼쪽으로 쉬프트       int extended = (int)(short)(value << (16 - bits)) >> (16 - bits);
    return (unsigned int)(extended >> shift);  // 오른쪽으로 쉬프트하여 부호 확장       return (unsigned int)extended;
}