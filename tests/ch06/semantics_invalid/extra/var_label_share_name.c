int main(void) {
    int a = 0;
    goto a;
    int b = 3;
a:
    b = 5 + a;
    return b;
}
