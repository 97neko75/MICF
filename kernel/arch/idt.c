#include "idt.h"
#include <common.h>

void idt_init(void) {
    // 暂时不设 IDT，所有中断都会导致 triple fault
    // 仅清空 IDTR
    __asm__ volatile ("lidt (%0)" : : "r" ((uint16_t[]){0,0}));
}