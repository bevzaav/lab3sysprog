int main()
{
    int a = 7, b = 3;
    print("a + b * 2 =", a + b * 2);
    print("(a + b) * 2 =", (a + b) * 2);
    print("a / b, a % b =", a / b, a % b);
    print("-a / b =", -a / b, " -a % b =", -a % b);
    print("a << 2, a & b, a | b, a ^ b =", a << 2, a & b, a | b, a ^ b);
    print("a > b && b > 0 :", a > b && b > 0);
    print("ternary:", a > b ? a : b);
    return 0;
}
