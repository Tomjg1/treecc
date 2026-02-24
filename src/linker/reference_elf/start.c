const int rodata[] = { 1, 2 };
static int data[] = { 1, 2 };

void _start() {
    __asm__ volatile("call main\n"
                     "mov %rax, %rdi\n"
                     "mov $60, %rax\n"
                     "syscall\n");
}

int main() {
    return 0;
}
