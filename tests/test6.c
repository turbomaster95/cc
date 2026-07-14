int main() {
    int x = 0;
    asm("movq $42, %rax");
    return x;
}
