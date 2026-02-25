int a;
static int b;
const int c;
extern int d;
void func ();
static void dunc(void) {
    func();
    dunc();
    int a = d;
}


static void runc(void) {
    dunc();
}

void _start(void) {
        for(;;);
        __asm__ volatile(
            "movq $60, %%rax\n"   // sys_exit
            "movq $0, %%rdi\n"    // exit status
            "syscall\n"
            :
            :
            : "%rax", "%rdi"
        );
}
