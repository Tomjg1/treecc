int a;
static int b;
const int c;
extern int d;
void func ();

char weak __attribute__((weak)) = '1';

__thread int thread_test;
__thread int thread_test_super = 5;

static void dunc(void) {
    func();
    dunc();
    int a = d;
}

static void runc(void) {
    dunc();
}

void cunc(void) {

}

int main(void) {
    char msg[] = "  Hello, World!\n";
    msg[0] = weak;
    __asm__ volatile (
        "mov $1, %%rax\n"       // write syscall
        "mov $1, %%rdi\n"       // stdout
        "mov %0, %%rsi\n"       // buf from C variable
        "mov $16, %%rdx\n"      // length
        "syscall\n"
        :
        : "r"(msg)
        : "rax", "rdi", "rsi", "rdx"
    );
    thread_test = 5;
    thread_test_super = 6;
    __asm__ volatile(
        "movq $60, %%rax\n"   // sys_exit
        "movq $0, %%rdi\n"    // exit status
        "syscall\n"
        :
        :
        : "%rax", "%rdi"
    );
    return 1;
}
