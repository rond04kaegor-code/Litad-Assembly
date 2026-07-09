// lasm.c - Litad Assembly Compiler
// Full Long Mode Support for x86_64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

#define MAX_LINE 4096
#define MAX_LABELS 65536
#define CODE_SIZE 1048576
#define VERSION "1.0"

// Forward declarations
void emit_byte(uint8_t byte);
void emit_word(uint16_t word);
void emit_dword(uint32_t dword);
void emit_qword(uint64_t qword);
uint64_t parse_number(const char *str);

// Long Mode structures
typedef struct {
    uint64_t pml4[512];
} PML4_TABLE;

typedef struct {
    uint64_t limit_low : 16;
    uint64_t base_low : 24;
    uint64_t accessed : 1;
    uint64_t read_write : 1;
    uint64_t conforming : 1;
    uint64_t code : 1;
    uint64_t code_data_segment : 1;
    uint64_t DPL : 2;
    uint64_t present : 1;
    uint64_t limit_high : 4;
    uint64_t available : 1;
    uint64_t long_mode : 1;
    uint64_t big : 1;
    uint64_t gran : 1;
    uint64_t base_high : 8;
} __attribute__((packed)) GDT_ENTRY;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) GDTR;

typedef struct {
    char name[256];
    uint64_t address;
    int defined;
} Label;

typedef struct {
    uint8_t *code;
    size_t size;
    size_t capacity;
    uint64_t origin;
    int bits;
} CodeBuffer;

// Global variables
Label labels[MAX_LABELS];
int label_count = 0;
CodeBuffer code_buf;
int current_pass = 1;
uint64_t current_address = 0;
time_t compile_time;

// Supported CPUs
const char* supported_cpus[] = {
    "INTEL_CORE", "INTEL_XEON", "AMD_RYZEN", "AMD_EPYC",
    "ELBRUS_8C", "ELBRUS_16C", "BAIKAL_M", "BAIKAL_S",
    "MCST_R1000", "MCST_R2000", NULL
};

// Initialize code buffer
void init_code_buffer(CodeBuffer *buf, uint64_t origin) {
    buf->capacity = CODE_SIZE;
    buf->code = (uint8_t*)malloc(buf->capacity);
    if (!buf->code) {
        fprintf(stderr, "LASM: Memory allocation failed\n");
        exit(1);
    }
    buf->size = 0;
    buf->origin = origin;
    buf->bits = 64;
    current_address = origin;
    compile_time = time(NULL);
}

// Emit functions
void emit_byte(uint8_t byte) {
    if (code_buf.size >= code_buf.capacity) {
        code_buf.capacity *= 2;
        uint8_t *new_code = (uint8_t*)realloc(code_buf.code, code_buf.capacity);
        if (!new_code) {
            fprintf(stderr, "LASM: Memory reallocation failed\n");
            exit(1);
        }
        code_buf.code = new_code;
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

// Long Mode instructions
void assemble_mov_rax_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xB8);
    emit_qword(imm);
}

void assemble_mov_rcx_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xB9);
    emit_qword(imm);
}

void assemble_mov_rdx_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xBA);
    emit_qword(imm);
}

void assemble_mov_rbx_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xBB);
    emit_qword(imm);
}

void assemble_mov_rsp_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xBC);
    emit_qword(imm);
}

void assemble_mov_rbp_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xBD);
    emit_qword(imm);
}

void assemble_mov_rsi_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xBE);
    emit_qword(imm);
}

void assemble_mov_rdi_imm64(uint64_t imm) {
    emit_byte(0x48);
    emit_byte(0xBF);
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

void assemble_nop() {
    emit_byte(0x90);
}

void assemble_syscall() {
    emit_byte(0x0F);
    emit_byte(0x05);
}

void assemble_sysret() {
    emit_byte(0x0F);
    emit_byte(0x07);
}

void assemble_iretq() {
    emit_byte(0x48);
    emit_byte(0xCF);
}

void assemble_rdmsr() {
    emit_byte(0x0F);
    emit_byte(0x32);
}

void assemble_wrmsr() {
    emit_byte(0x0F);
    emit_byte(0x30);
}

void assemble_rdtsc() {
    emit_byte(0x0F);
    emit_byte(0x31);
}

void assemble_cpuid() {
    emit_byte(0x0F);
    emit_byte(0xA2);
}

void assemble_mov_cr0_rax() {
    emit_byte(0x0F);
    emit_byte(0x22);
    emit_byte(0xC0);
}

void assemble_mov_rax_cr0() {
    emit_byte(0x0F);
    emit_byte(0x20);
    emit_byte(0xC0);
}

void assemble_mov_cr3_rax() {
    emit_byte(0x0F);
    emit_byte(0x22);
    emit_byte(0xD8);
}

void assemble_mov_rax_cr3() {
    emit_byte(0x0F);
    emit_byte(0x20);
    emit_byte(0xD8);
}

void assemble_mov_cr4_rax() {
    emit_byte(0x0F);
    emit_byte(0x22);
    emit_byte(0xE0);
}

void assemble_mov_rax_cr4() {
    emit_byte(0x0F);
    emit_byte(0x20);
    emit_byte(0xE0);
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

void assemble_jmp_short(uint8_t offset) {
    emit_byte(0xEB);
    emit_byte(offset);
}

void assemble_call_rel32(uint32_t offset) {
    emit_byte(0xE8);
    emit_dword(offset);
}

// Parse number with multiple formats
uint64_t parse_number(const char *str) {
    if (!str) return 0;
    
    while (isspace((unsigned char)*str)) str++;
    
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return strtoull(str + 2, NULL, 16);
    }
    if (str[0] == '0' && (str[1] == 'b' || str[1] == 'B')) {
        return strtoull(str + 2, NULL, 2);
    }
    
    return strtoull(str, NULL, 10);
}

// Label management
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

// Enable Long Mode code generation
void enable_long_mode_code() {
    // Set up page tables for Long Mode
    assemble_mov_rax_imm64(0x1000);
    assemble_mov_cr3_rax();
    
    // Enable PAE
    assemble_mov_rax_cr4();
    emit_byte(0x48);
    emit_byte(0x0D);
    emit_dword(0x00000020);
    assemble_mov_cr4_rax();
    
    // Enable Long Mode in EFER
    assemble_mov_rcx_imm64(0xC0000080);
    assemble_rdmsr();
    emit_byte(0x48);
    emit_byte(0x0D);
    emit_dword(0x00000100);
    assemble_wrmsr();
    
    // Enable paging
    assemble_mov_rax_cr0();
    emit_byte(0x48);
    emit_byte(0x0D);
    emit_dword(0x80000001);
    assemble_mov_cr0_rax();
}

// Handle directives
void handle_bits(const char *line) {
    int bits;
    if (sscanf(line, "BITS %d", &bits) == 1) {
        if (bits == 16 || bits == 32 || bits == 64) {
            code_buf.bits = bits;
            printf("LASM: Set bits mode to %d\n", bits);
        }
    }
}

void handle_org(const char *line) {
    uint64_t addr;
    char hex_str[64];
    if (sscanf(line, "[ORG %s]", hex_str) == 1) {
        addr = strtoull(hex_str, NULL, 16);
        code_buf.origin = addr;
        current_address = addr;
        printf("LASM: Set origin to 0x%llX\n", (unsigned long long)addr);
    }
}

void handle_long_mode() {
    printf("LASM: Enabling Long Mode support...\n");
    enable_long_mode_code();
}

void handle_echo(const char *str) {
    printf("%s", str);
}

// Assemble boot sector with Long Mode
void assemble_long_mode_boot_sector() {
    // JMP to boot code
    emit_byte(0xEB);
    emit_byte(0x3C);
    emit_byte(0x90);
    
    // OEM
    const char *oem = "LASM1.0 ";
    for (int i = 0; i < 8; i++) emit_byte(oem[i]);
    
    // BPB
    emit_word(512);
    emit_byte(1);
    emit_word(1);
    emit_byte(0);
    emit_word(0);
    emit_byte(0xF8);
    emit_word(0);
    emit_word(0);
    emit_word(0);
    emit_dword(0);
    emit_dword(0);
    
    // More BPB
    emit_byte(0);
    emit_byte(0);
    emit_byte(0x29);
    emit_dword(0x12345678);
    for (int i = 0; i < 11; i++) emit_byte(' ');
    for (int i = 0; i < 8; i++) emit_byte(' ');
    
    // Boot code
    assemble_cli();
    assemble_xor_rax_rax();
    
    // Set up stack
    assemble_mov_rsp_imm64(0x7C00);
    
    // Enable A20 line
    assemble_mov_rax_imm64(0x2401);
    assemble_int_imm8(0x15);
    
    // Load kernel
    assemble_mov_rax_imm64(0x0201);
    assemble_mov_rcx_imm64(0x0002);
    assemble_mov_rdx_imm64(0x0080);
    assemble_mov_rbx_imm64(0x1000);
    assemble_int_imm8(0x13);
    
    // Enter Long Mode
    enable_long_mode_code();
    
    // Pad to 510 bytes
    while (code_buf.size < 510) {
        emit_byte(0x00);
    }
    
    // Boot signature
    emit_byte(0x55);
    emit_byte(0xAA);
}

// Main assemble function
void assemble_line(char *line_buffer) {
    char trimmed[MAX_LINE];
    strcpy(trimmed, line_buffer);
    
    char *trim_ptr = trimmed;
    while (isspace((unsigned char)*trim_ptr)) trim_ptr++;
    
    char *end = trim_ptr + strlen(trim_ptr) - 1;
    while (end > trim_ptr && isspace((unsigned char)*end)) *end-- = '\0';
    
    if (strlen(trim_ptr) == 0) return;
    
    if (trim_ptr[0] == ';' || trim_ptr[0] == '#') return;
    
    // VERSION
    if (strcmp(trim_ptr, "VERSION") == 0) {
        printf("LASM Version: %s\n", VERSION);
        printf("Build time: %s", ctime(&compile_time));
        return;
    }
    
    // BITS directive
    if (strncmp(trim_ptr, "BITS", 4) == 0) {
        handle_bits(trim_ptr);
        return;
    }
    
    // ORG directive
    if (strncmp(trim_ptr, "[ORG", 4) == 0) {
        handle_org(trim_ptr);
        return;
    }
    
    // LONG_MODE directive
    if (strcmp(trim_ptr, "LONG_MODE") == 0 || strcmp(trim_ptr, "LONGMODE") == 0) {
        handle_long_mode();
        return;
    }
    
    // Label:
    char *colon = strchr(trim_ptr, ':');
    if (colon) {
        *colon = '\0';
        add_label(trim_ptr, current_address, 1);
        char *rest = colon + 1;
        while (isspace((unsigned char)*rest)) rest++;
        if (*rest) {
            assemble_line(rest);
        }
        return;
    }
    
    // ECHO "string"
    if (strncmp(trim_ptr, "ECHO", 4) == 0) {
        char *start = strchr(trim_ptr, '"');
        if (start) {
            start++;
            char *end_str = strchr(start, '"');
            if (end_str) {
                *end_str = '\0';
                handle_echo(start);
            }
        }
        return;
    }
    
    // MB(!val!)
    if (strstr(trim_ptr, "MB")) {
        char *macro_start = strstr(trim_ptr, "(!");
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
    
    // BOOT()
    if (strcmp(trim_ptr, "BOOT()") == 0 || strcmp(trim_ptr, "boot()") == 0) {
        assemble_long_mode_boot_sector();
        return;
    }
    
    // MOV instruction
    if (strncmp(trim_ptr, "MOV", 3) == 0) {
        char dest[32], src[256];
        if (sscanf(trim_ptr, "MOV %[^,], %[^\n]", dest, src) == 2) {
            char *dptr = dest;
            while (isspace((unsigned char)*dptr)) dptr++;
            char *dend = dptr + strlen(dptr) - 1;
            while (dend > dptr && isspace((unsigned char)*dend)) *dend-- = '\0';
            
            char *sptr = src;
            while (isspace((unsigned char)*sptr)) sptr++;
            
            uint64_t val = parse_number(sptr);
            
            if (strcasecmp(dptr, "RAX") == 0) assemble_mov_rax_imm64(val);
            else if (strcasecmp(dptr, "RCX") == 0) assemble_mov_rcx_imm64(val);
            else if (strcasecmp(dptr, "RDX") == 0) assemble_mov_rdx_imm64(val);
            else if (strcasecmp(dptr, "RBX") == 0) assemble_mov_rbx_imm64(val);
            else if (strcasecmp(dptr, "RSP") == 0) assemble_mov_rsp_imm64(val);
            else if (strcasecmp(dptr, "RBP") == 0) assemble_mov_rbp_imm64(val);
            else if (strcasecmp(dptr, "RSI") == 0) assemble_mov_rsi_imm64(val);
            else if (strcasecmp(dptr, "RDI") == 0) assemble_mov_rdi_imm64(val);
        }
        return;
    }
    
    // XOR RAX, RAX
    if (strstr(trim_ptr, "XOR") && strstr(trim_ptr, "RAX")) {
        assemble_xor_rax_rax();
        return;
    }
    
    // System instructions
    if (strcasecmp(trim_ptr, "SYSCALL") == 0) { assemble_syscall(); return; }
    if (strcasecmp(trim_ptr, "SYSRET") == 0) { assemble_sysret(); return; }
    if (strcasecmp(trim_ptr, "IRETQ") == 0) { assemble_iretq(); return; }
    if (strcasecmp(trim_ptr, "RDMSR") == 0) { assemble_rdmsr(); return; }
    if (strcasecmp(trim_ptr, "WRMSR") == 0) { assemble_wrmsr(); return; }
    if (strcasecmp(trim_ptr, "RDTSC") == 0) { assemble_rdtsc(); return; }
    if (strcasecmp(trim_ptr, "CPUID") == 0) { assemble_cpuid(); return; }
    
    // Control registers
    if (strcasecmp(trim_ptr, "MOV CR0, RAX") == 0) { assemble_mov_cr0_rax(); return; }
    if (strcasecmp(trim_ptr, "MOV RAX, CR0") == 0) { assemble_mov_rax_cr0(); return; }
    if (strcasecmp(trim_ptr, "MOV CR3, RAX") == 0) { assemble_mov_cr3_rax(); return; }
    if (strcasecmp(trim_ptr, "MOV RAX, CR3") == 0) { assemble_mov_rax_cr3(); return; }
    if (strcasecmp(trim_ptr, "MOV CR4, RAX") == 0) { assemble_mov_cr4_rax(); return; }
    if (strcasecmp(trim_ptr, "MOV RAX, CR4") == 0) { assemble_mov_rax_cr4(); return; }
    
    // Basic instructions
    if (strcasecmp(trim_ptr, "RET") == 0) { assemble_ret(); return; }
    if (strcasecmp(trim_ptr, "HLT") == 0) { assemble_hlt(); return; }
    if (strcasecmp(trim_ptr, "CLI") == 0) { assemble_cli(); return; }
    if (strcasecmp(trim_ptr, "STI") == 0) { assemble_sti(); return; }
    if (strcasecmp(trim_ptr, "NOP") == 0) { assemble_nop(); return; }
    
    // INT imm8
    if (strncmp(trim_ptr, "INT", 3) == 0) {
        char imm_str[16];
        if (sscanf(trim_ptr, "INT %s", imm_str) == 1) {
            assemble_int_imm8((uint8_t)parse_number(imm_str));
        }
        return;
    }
    
    // JMP
    if (strncmp(trim_ptr, "JMP", 3) == 0) {
        char label_name[256];
        if (sscanf(trim_ptr, "JMP %s", label_name) == 1) {
            Label *lbl = find_label(label_name);
            if (lbl && lbl->defined) {
                int32_t offset = lbl->address - (current_address + 2);
                assemble_jmp_short((int8_t)offset);
            } else {
                assemble_jmp_short(0x00);
            }
        }
        return;
    }
    
    // CALL
    if (strncmp(trim_ptr, "CALL", 4) == 0) {
        assemble_call_rel32(0x00);
        return;
    }
    
    // PUSH/POP
    if (strcasecmp(trim_ptr, "PUSH RAX") == 0) { assemble_push_rax(); return; }
    if (strcasecmp(trim_ptr, "POP RAX") == 0) { assemble_pop_rax(); return; }
    if (strcasecmp(trim_ptr, "PUSH RBP") == 0) { assemble_push_rbp(); return; }
    if (strcasecmp(trim_ptr, "MOV RBP, RSP") == 0) { assemble_mov_rbp_rsp(); return; }
    
    // TIME
    if (strcasecmp(trim_ptr, "TIME") == 0) {
        emit_qword((uint64_t)compile_time);
        return;
    }
    
    // DB/DW/DD/DQ
    if (strncmp(trim_ptr, "DB", 2) == 0) {
        char *vals = strchr(trim_ptr, ' ');
        if (vals) {
            vals++;
            char *token = strtok(vals, ",");
            while (token) {
                char *tptr = token;
                while (isspace((unsigned char)*tptr)) tptr++;
                if (tptr[0] == '\'') {
                    emit_byte(tptr[1]);
                } else {
                    emit_byte((uint8_t)parse_number(tptr));
                }
                token = strtok(NULL, ",");
            }
        }
        return;
    }
    if (strncmp(trim_ptr, "DW", 2) == 0) {
        char *vals = strchr(trim_ptr, ' ');
        if (vals) {
            emit_word((uint16_t)parse_number(vals + 1));
        }
        return;
    }
    if (strncmp(trim_ptr, "DD", 2) == 0) {
        char *vals = strchr(trim_ptr, ' ');
        if (vals) {
            emit_dword((uint32_t)parse_number(vals + 1));
        }
        return;
    }
    if (strncmp(trim_ptr, "DQ", 2) == 0) {
        char *vals = strchr(trim_ptr, ' ');
        if (vals) {
            emit_qword(parse_number(vals + 1));
        }
        return;
    }
}

// Write binary output
void write_binary(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "LASM: Cannot write to %s\n", filename);
        return;
    }
    fwrite(code_buf.code, 1, code_buf.size, fp);
    fclose(fp);
    printf("LASM: Written %zu bytes to %s\n", code_buf.size, filename);
}

// Write ELF64
void write_elf64(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return;
    
    // ELF64 Header
    unsigned char elf_header[64] = {
        0x7F, 'E', 'L', 'F',
        2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        2, 0,
        0x3E, 0,
        1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        64, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    };
    
    fwrite(elf_header, 1, 64, fp);
    
    // Program header
    uint64_t phdr[7];
    phdr[0] = 1;      // p_type = PT_LOAD
    phdr[1] = 7;      // p_flags = RWX
    phdr[2] = 64 + 56; // p_offset
    phdr[3] = code_buf.origin; // p_vaddr
    phdr[4] = code_buf.origin; // p_paddr
    phdr[5] = code_buf.size;   // p_filesz
    phdr[6] = code_buf.size;   // p_memsz
    
    fwrite(phdr, 8, 7, fp);
    
    // Code section
    fwrite(code_buf.code, 1, code_buf.size, fp);
    fclose(fp);
    
    printf("LASM: Created ELF64: %s\n", filename);
}

// Main
int main(int argc, char **argv) {
    printf("Litad Assembly (LASM) v%s - Long Mode + time_t\n", VERSION);
    
    if (argc < 2) {
        printf("Supported CPUs:\n");
        for (int i = 0; supported_cpus[i]; i++) {
            printf("  - %s\n", supported_cpus[i]);
        }
        printf("\nUsage: %s <input.asm> [output] [--elf]\n", argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    const char *output_file = (argc > 2) ? argv[2] : "output.bin";
    int elf_mode = (argc > 3 && strcmp(argv[3], "--elf") == 0);
    
    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        fprintf(stderr, "LASM: Cannot open %s\n", input_file);
        return 1;
    }
    
    init_code_buffer(&code_buf, 0x7990);
    
    printf("LASM: Assembling %s\n", input_file);
    printf("LASM: time_t = %lu\n", (unsigned long)compile_time);
    
    char line[MAX_LINE];
    
    // Pass 1
    printf("LASM: Pass 1 (labels)...\n");
    current_pass = 1;
    current_address = code_buf.origin;
    code_buf.size = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        assemble_line(line);
    }
    
    // Pass 2
    printf("LASM: Pass 2 (code generation)...\n");
    current_pass = 2;
    current_address = code_buf.origin;
    code_buf.size = 0;
    rewind(fp);
    
    while (fgets(line, sizeof(line), fp)) {
        assemble_line(line);
    }
    
    fclose(fp);
    
    if (elf_mode) {
        write_elf64(output_file);
    } else {
        write_binary(output_file);
    }
    
    printf("LASM: Done. Output: %s\n", output_file);
    
    free(code_buf.code);
    return 0;
}
