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
