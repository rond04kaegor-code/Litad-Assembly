// liblasm.c - OS Development Library for LASM v1.0
// Полная поддержка Long Mode
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// time_t для Long Mode (64-битный)
typedef int64_t lasm_time_t;

// VGA Text Mode (работает в Long Mode)
#define VGA_ADDRESS 0xFFFFFFFF800B8000ULL
static uint16_t* const vga_buffer = (uint16_t*)VGA_ADDRESS;
static int vga_row = 0, vga_col = 0;

// Порты ввода-вывода для Long Mode
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

// VGA функции
void vga_clear_screen() {
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = (0x0F << 8) | ' ';
    }
    vga_row = 0;
    vga_col = 0;
}

void vga_putchar(char c, uint8_t color) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        return;
    }
    
    vga_buffer[vga_row * 80 + vga_col] = (color << 8) | c;
    vga_col++;
    
    if (vga_col >= 80) {
        vga_col = 0;
        vga_row++;
    }
}

void vga_print(const char *str) {
    while (*str) {
        vga_putchar(*str++, 0x0F);
    }
}

// Структуры для Long Mode
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

// Управление памятью в Long Mode
#define PAGE_SIZE 4096
#define PAGE_PRESENT 0x1
#define PAGE_WRITE 0x2
#define PAGE_USER 0x4

uint64_t* pml4_table;

void init_paging() {
    pml4_table = (uint64_t*)0x1000;
    
    // Очистка PML4
    for (int i = 0; i < 512; i++) {
        pml4_table[i] = 0;
    }
    
    // Идентичное отображение первых 2MB
    uint64_t* pdpt = (uint64_t*)0x2000;
    uint64_t* pd = (uint64_t*)0x3000;
    
    pml4_table[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITE;
    pdpt[0] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
    
    // 2MB страницы
    for (int i = 0; i < 512; i++) {
        pd[i] = (i * 0x200000) | PAGE_PRESENT | PAGE_WRITE | 0x80; // PS bit
    }
    
    // Загрузка PML4 в CR3
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4_table));
}

// Системные вызовы Long Mode
#define SYS_WRITE 1
#define SYS_READ 0
#define SYS_EXIT 60

static inline long syscall(long number, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// Функции времени для Long Mode
lasm_time_t lasm_time(lasm_time_t *t) {
    lasm_time_t current_time;
    
    // Чтение RTC через порты CMOS
    outb(0x70, 0x00);
    uint8_t second = inb(0x71);
    
    outb(0x70, 0x02);
    uint8_t minute = inb(0x71);
    
    outb(0x70, 0x04);
    uint8_t hour = inb(0x71);
    
    outb(0x70, 0x07);
    uint8_t day = inb(0x71);
    
    outb(0x70, 0x08);
    uint8_t month = inb(0x71);
    
    outb(0x70, 0x09);
    uint8_t year = inb(0x71);
    
    // Конвертация BCD в бинарный
    second = (second & 0x0F) + ((second / 16) * 10);
    minute = (minute & 0x0F) + ((minute / 16) * 10);
    hour = (hour & 0x0F) + ((hour / 16) * 10);
    day = (day & 0x0F) + ((day / 16) * 10);
    month = (month & 0x0F) + ((month / 16) * 10);
    year = (year & 0x0F) + ((year / 16) * 10) + 2000;
    
    // Вычисление time_t (упрощенно)
    current_time = (year - 1970) * 365 * 24 * 3600;
    current_time += (month - 1) * 30 * 24 * 3600;
    current_time += (day - 1) * 24 * 3600;
    current_time += hour * 3600 + minute * 60 + second;
    
    if (t) *t = current_time;
    return current_time;
}

// Инициализация Long Mode
void init_long_mode() {
    vga_clear_screen();
    vga_print("LASM OS v1.0 - Long Mode Initialized\r\n");
    vga_print("time_t supported\r\n");
    
    // Инициализация страничной адресации
    init_paging();
    
    // Вывод текущего времени
    lasm_time_t now = lasm_time(NULL);
    vga_print("Current time_t: ");
    // Конвертация time_t в строку
    char time_str[32];
    int hours = (now % 86400) / 3600;
    int minutes = (now % 3600) / 60;
    int seconds = now % 60;
    // Здесь можно добавить код вывода времени
}

// Экспортируемые функции
void lasm_init() {
    init_long_mode();
}

void lasm_print(const char *str) {
    vga_print(str);
}

void lasm_halt() {
    vga_print("\r\nSystem halted.");
    __asm__ volatile ("hlt");
    while(1);
}
