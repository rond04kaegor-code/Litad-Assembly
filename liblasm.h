// lasm.c - Litad Assembly Compiler
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_LINE 4096
#define MAX_LABELS 65536
#define MAX_MACROS 4096
#define CODE_SIZE 1048576

typedef struct {
    char name[256];
    uint64_t address;
    int defined;
} Label;

typedef struct {
    char name[256];
    char body[MAX_LINE];
} Macro;

typedef struct {
    uint8_t *code;
    size_t size;
    size_t capacity;
    uint64_t origin;
    int bits;
} CodeBuffer;

Label labels[MAX_LABELS];
Macro macros[MAX_MACROS];
int label_count = 0;
int macro_count = 0;
CodeBuffer code_buf;
int current_pass = 1;
uint64_t current_address = 0;

// Intel/AMD и российские процессоры поддерживают x86_64
const char* supported_cpus[] = {
    "INTEL", "AMD", "ELBRUS", "BAIKAL", "MCST", NULL
};

void init_code_buffer(CodeBuffer *buf, uint64_t origin) {
    buf->capacity = CODE_SIZE;
    buf->code = (uint8_t*)malloc(buf->capacity);
    buf->size = 0;
    buf->origin = origin;
    buf->bits = 64;
    current_address = origin;
}

void emit_byte(uint8_t byte) {
    if (code_buf.size >= code_buf.capacity) {
        code_buf.capacity *= 2;
        code_buf.code = (uint8_t*)realloc(code_buf.code, code_buf.capacity);
    }
    code_buf.code[code_buf.size++] = byte;
    current_address++;
}

void emit_word(uint16_t word) {
    emit_byte(word & 0xFF);
    emit_byte((word >> 8) & 0xFF);
}

void emit_dword(uint32_t dword) {
    emit_byte(dword & 0xFF);
    emit_byte((dword >> 8) & 0xFF);
    emit_byte((dword >> 16) & 0xFF);
    emit_byte((dword >> 24) & 0xFF);
}

void emit_qword(uint64_t qword) {
    emit_dword(qword & 0xFFFFFFFF);
    emit_dword((qword >> 32) & 0xFFFFFFFF);
}

// LASM инструкции
void assemble_mov_rax_imm64(uint64_t imm) {
    emit_byte(0x48); // REX.W
    emit_byte(0xB8); // MOV RAX, imm64
    emit_qword(imm);
}

void assemble_mov_rcx_imm64(uint64_t imm) {
    emit_byte(0x48); // REX.W
    emit_byte(0xB9); // MOV RCX, imm64
    emit_qword(imm);
}

void assemble_mov_rdx_imm64(uint64_t imm) {
    emit_byte(0x48); // REX.W
    emit_byte(0xBA); // MOV RDX, imm64
    emit_qword(imm);
}

void assemble_xor_rax_rax() {
    emit_byte(0x48);
    emit_byte(0x31);
    emit_byte(0xC0);
}

void assemble_ret() {
    emit_byte(0xC3);
}

void assemble_int_imm8(uint8_t imm) {
    emit_byte(0xCD);
    emit_byte(imm);
}

void assemble_hlt() {
    emit_byte(0xF4);
}

void assemble_cli() {
    emit_byte(0xFA);
}

void assemble_sti() {
    emit_byte(0xFB);
}

void assemble_jmp_rel32(uint32_t offset) {
    emit_byte(0xE9);
    emit_dword(offset);
}

void assemble_jmp_short(uint8_t offset) {
    emit_byte(0xEB);
    emit_byte(offset);
}

void assemble_call_rel32(uint32_t offset) {
    emit_byte(0xE8);
    emit_dword(offset);
}

void assemble_push_rax() {
    emit_byte(0x50);
}

void assemble_pop_rax() {
    emit_byte(0x58);
}

void assemble_push_rbp() {
    emit_byte(0x55);
}

void assemble_mov_rbp_rsp() {
    emit_byte(0x48);
    emit_byte(0x89);
    emit_byte(0xE5);
}

// Обработка псевдоинструкции ECHO
void handle_echo(const char *str) {
    printf("%s", str);
}

// Парсинг числа
uint64_t parse_number(const char *str) {
    if (str[0] == '0' && str[1] == 'x') {
        return strtoull(str + 2, NULL, 16);
    }
    return strtoull(str, NULL, 10);
}

// Поиск метки
Label* find_label(const char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            return &labels[i];
        }
    }
    return NULL;
}

int add_label(const char *name, uint64_t address, int defined) {
    Label *existing = find_label(name);
    if (existing) {
        if (!existing->defined && defined) {
            existing->address = address;
            existing->defined = 1;
        }
        return 1;
    }
    
    if (label_count >= MAX_LABELS) return 0;
    
    strcpy(labels[label_count].name, name);
    labels[label_count].address = address;
    labels[label_count].defined = defined;
    label_count++;
    return 1;
}

// Обработка директивы BITS
void handle_bits(const char *line) {
    int bits;
    if (sscanf(line, "BITS %d", &bits) == 1) {
        if (bits == 16 || bits == 32 || bits == 64) {
            code_buf.bits = bits;
            printf("LASM: Set bits mode to %d\n", bits);
        }
    }
}

// Обработка директивы ORG
void handle_org(const char *line) {
    uint64_t addr;
    char hex_str[64];
    if (sscanf(line, "[ORG %s]", hex_str) == 1) {
        if (strncmp(hex_str, "0x", 2) == 0) {
            addr = strtoull(hex_str + 2, NULL, 16);
        } else {
            addr = strtoull(hex_str, NULL, 16);
        }
        code_buf.origin = addr;
        current_address = addr;
        printf("LASM: Set origin to 0x%llX\n", (unsigned long long)addr);
    }
}

// Обработка BOOT сектора
void assemble_boot_sector() {
    // Стандартный загрузочный сектор
    emit_byte(0xEB); // JMP SHORT
    emit_byte(0x3C); // Смещение к началу кода
    emit_byte(0x90); // NOP
    
    // BPB (BIOS Parameter Block) - нули
    for (int i = 0; i < 8; i++) emit_byte(0x00);
    
    // Больше BPB данных
    for (int i = 0; i < 50; i++) emit_byte(0x00);
    
    // Конец загрузочного сектора с сигнатурой
    while (code_buf.size < 510) {
        emit_byte(0x00);
    }
    emit_byte(0x55);
    emit_byte(0xAA);
}

// Основной ассемблер
void assemble_line(const char *line) {
    char trimmed[MAX_LINE];
    strcpy(trimmed, line);
    
    // Удаляем лишние пробелы
    while (isspace(*trimmed)) trimmed++;
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && isspace(*end)) *end-- = '\0';
    
    if (strlen(trimmed) == 0) return;
    
    // Комментарий
    if (trimmed[0] == ';' || trimmed[0] == '#') return;
    
    // Директива BITS
    if (strncmp(trimmed, "BITS", 4) == 0) {
        handle_bits(trimmed);
        return;
    }
    
    // Директива ORG
    if (strncmp(trimmed, "[ORG", 4) == 0) {
        handle_org(trimmed);
        return;
    }
    
    // МЕТКА:
    char *colon = strchr(trimmed, ':');
    if (colon) {
        *colon = '\0';
        add_label(trimmed, current_address, 1);
        if (*(colon + 1)) {
            assemble_line(colon + 1);
        }
        return;
    }
    
    // ECHO "строка"
    if (strncmp(trimmed, "ECHO", 4) == 0) {
        char *start = strchr(trimmed, '"');
        if (start) {
            start++;
            char *end = strchr(start, '"');
            if (end) {
                *end = '\0';
                handle_echo(start);
            }
        }
        return;
    }
    
    // MB(!10!) - макро
    if (strstr(trimmed, "MB")) {
        char *macro_start = strstr(trimmed, "(!");
        if (macro_start) {
            macro_start += 2;
            char *macro_end = strstr(macro_start, "!)");
            if (macro_end) {
                *macro_end = '\0';
                uint64_t val = parse_number(macro_start);
                emit_byte((uint8_t)val);
            }
        }
        return;
    }
    
    // BOOT() - загрузочный сектор
    if (strcmp(trimmed, "BOOT()") == 0 || strcmp(trimmed, "boot()") == 0) {
        assemble_boot_sector();
        return;
    }
    
    // Инструкции MOV
    if (strncmp(trimmed, "MOV", 3) == 0) {
        char dest[32], src[256];
        if (sscanf(trimmed, "MOV %[^,], %[^\n]", dest, src) == 2) {
            // Убираем пробелы
            while (isspace(*dest)) dest++;
            char *dend = dest + strlen(dest) - 1;
            while (dend > dest && isspace(*dend)) *dend-- = '\0';
            while (isspace(*src)) src++;
            
            if (strcmp(dest, "RAX") == 0 || strcmp(dest, "rax") == 0) {
                uint64_t val = parse_number(src);
                assemble_mov_rax_imm64(val);
            } else if (strcmp(dest, "RCX") == 0 || strcmp(dest, "rcx") == 0) {
                uint64_t val = parse_number(src);
                assemble_mov_rcx_imm64(val);
            } else if (strcmp(dest, "RDX") == 0 || strcmp(dest, "rdx") == 0) {
                uint64_t val = parse_number(src);
                assemble_mov_rdx_imm64(val);
            }
        }
        return;
    }
    
    // XOR RAX, RAX
    if (strstr(trimmed, "XOR") && strstr(trimmed, "RAX")) {
        assemble_xor_rax_rax();
        return;
    }
    
    // RET
    if (strcmp(trimmed, "RET") == 0 || strcmp(trimmed, "ret") == 0) {
        assemble_ret();
        return;
    }
    
    // INT
    if (strncmp(trimmed, "INT", 3) == 0) {
        char imm_str[16];
        if (sscanf(trimmed, "INT %s", imm_str) == 1) {
            uint8_t imm = (uint8_t)parse_number(imm_str);
            assemble_int_imm8(imm);
        }
        return;
    }
    
    // HLT
    if (strcmp(trimmed, "HLT") == 0 || strcmp(trimmed, "hlt") == 0) {
        assemble_hlt();
        return;
    }
    
    // CLI/STI
    if (strcmp(trimmed, "CLI") == 0 || strcmp(trimmed, "cli") == 0) {
        assemble_cli();
        return;
    }
    if (strcmp(trimmed, "STI") == 0 || strcmp(trimmed, "sti") == 0) {
        assemble_sti();
        return;
    }
    
    // JMP метка
    if (strncmp(trimmed, "JMP", 3) == 0) {
        char label_name[256];
        if (sscanf(trimmed, "JMP %s", label_name) == 1) {
            Label *lbl = find_label(label_name);
            if (lbl && lbl->defined) {
                int32_t offset = lbl->address - (current_address + 2);
                assemble_jmp_short((int8_t)offset);
            } else {
                // Заглушка для первого прохода
                assemble_jmp_short(0x00);
            }
        }
        return;
    }
    
    // CALL
    if (strncmp(trimmed, "CALL", 4) == 0) {
        char label_name[256];
        if (sscanf(trimmed, "CALL %s", label_name) == 1) {
            assemble_call_rel32(0x00);
        }
        return;
    }
    
    // PUSH/POP
    if (strcmp(trimmed, "PUSH RAX") == 0) { assemble_push_rax(); return; }
    if (strcmp(trimmed, "POP RAX") == 0) { assemble_pop_rax(); return; }
    if (strcmp(trimmed, "PUSH RBP") == 0) { assemble_push_rbp(); return; }
    if (strcmp(trimmed, "MOV RBP, RSP") == 0 || strcmp(trimmed, "mov rbp, rsp") == 0) {
        assemble_mov_rbp_rsp();
        return;
    }
    
    // DB (Define Byte)
    if (strncmp(trimmed, "DB", 2) == 0) {
        char *vals = strchr(trimmed, ' ');
        if (vals) {
            vals++;
            char *token = strtok(vals, ",");
            while (token) {
                while (isspace(*token)) token++;
                if (token[0] == '\'') {
                    emit_byte(token[1]);
                } else {
                    emit_byte((uint8_t)parse_number(token));
                }
                token = strtok(NULL, ",");
            }
        }
        return;
    }
    
    // DW (Define Word)
    if (strncmp(trimmed, "DW", 2) == 0) {
        char *vals = strchr(trimmed, ' ');
        if (vals) {
            vals++;
            emit_word((uint16_t)parse_number(vals));
        }
        return;
    }
    
    // DD (Define Doubleword)
    if (strncmp(trimmed, "DD", 2) == 0) {
        char *vals = strchr(trimmed, ' ');
        if (vals) {
            vals++;
            emit_dword((uint32_t)parse_number(vals));
        }
        return;
    }
    
    // DQ (Define Quadword)
    if (strncmp(trimmed, "DQ", 2) == 0) {
        char *vals = strchr(trimmed, ' ');
        if (vals) {
            vals++;
            emit_qword(parse_number(vals));
        }
        return;
    }
}

void write_binary(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("LASM: Error writing binary file\n");
        return;
    }
    fwrite(code_buf.code, 1, code_buf.size, fp);
    fclose(fp);
    printf("LASM: Written %zu bytes to %s\n", code_buf.size, filename);
}

void write_elf64(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return;
    
    // ELF Header
    uint8_t elf_header[] = {
        0x7F, 'E', 'L', 'F', // Magic
        2, // 64-bit
        1, // Little endian
        1, // ELF version
        0, // System V ABI
        0, // ABI version
        0,0,0,0,0,0,0, // Padding
        2,0, // ET_EXEC
        0x3E,0, // x86_64
        1,0,0,0, // ELF version
        0,0,0,0,0,0,0,0, // Entry point
        64,0,0,0,0,0,0,0, // Program header offset
        0,0,0,0,0,0,0,0 // Section header offset
    };
    
    fwrite(elf_header, 1, sizeof(elf_header), fp);
    
    // Program header
    uint64_t phdr[7] = {
        1, // PT_LOAD
        0, // Offset
        0x400000, // Virtual address
        0x400000, // Physical address
        code_buf.size, // File size
        code_buf.size, // Memory size
        7 // Flags (RWX)
    };
    
    fwrite(phdr, 1, 56, fp);
    fwrite(code_buf.code, 1, code_buf.size, fp);
    fclose(fp);
    
    printf("LASM: Created ELF64 binary: %s\n", filename);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Litad Assembly (LASM) v1.0\n");
        printf("Supported CPUs: Intel, AMD, Elbrus, Baikal, MCST\n");
        printf("Usage: %s <input.asm> [output.bin] [--elf]\n", argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    const char *output_file = (argc > 2) ? argv[2] : "output.bin";
    int elf_mode = (argc > 3 && strcmp(argv[3], "--elf") == 0);
    
    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        printf("LASM: Cannot open %s\n", input_file);
        return 1;
    }
    
    init_code_buffer(&code_buf, 0x7990);
    
    char line[MAX_LINE];
    
    // Первый проход
    printf("LASM: Pass 1...\n");
    current_pass = 1;
    current_address = code_buf.origin;
    code_buf.size = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        assemble_line(line);
    }
    
    // Второй проход для разрешения меток
    printf("LASM: Pass 2...\n");
    current_pass = 2;
    current_address = code_buf.origin;
    code_buf.size = 0;
    fseek(fp, 0, SEEK_SET);
    
    while (fgets(line, sizeof(line), fp)) {
        assemble_line(line);
    }
    
    fclose(fp);
    
    if (elf_mode) {
        write_elf64(output_file);
    } else {
        write_binary(output_file);
    }
    
    free(code_buf.code);
    return 0;
}
