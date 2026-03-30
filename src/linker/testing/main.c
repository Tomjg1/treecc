

__attribute__((section("CUSTOM_SECTION2"))) int test;

__attribute__((section("CUSTOM_SECTION"))) int far() {
    return 5;
}

int test2 = 5;
int data = 5;

int main(){
    return 0;
}
