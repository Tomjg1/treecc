
/*
void exit(void) {
    asm volatile(
        "movq $60, %%rax\n"   // sys_exit
        "movq $0, %%rdi\n"    // exit status
        "syscall\n"
        :
        :
        : "%rax", "%rdi"
    );
}
*/

void _start(void) {
    //exit();
}
