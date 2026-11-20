int total_calls = 0;

int gcd(int a, int b)
{
    total_calls++;
    return b == 0 ? a : gcd(b, a % b);
}

int main()
{
    print("gcd(48, 18) =", gcd(48, 18));
    print("gcd(1071, 462) =", gcd(1071, 462));
    print("calls made:", total_calls);
    return 0;
}
