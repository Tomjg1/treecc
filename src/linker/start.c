

void _start() {
    __asm__ volatile("call main\n"
                     "mov %rax, %rdi\n"
                     "mov $60, %rax\n"
                     "syscall\n");
}
