void func(void);

void func(void) {

}

void cunc(void) __attribute__((weak));

char weak = '2';

int d;
static int c = 5;
static void dunc() {

}
void test(void) {
    c = 4;
    func();
    dunc();
}
