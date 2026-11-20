int is_prime(int n)
{
    if (n < 2)
        return 0;
    for (int d = 2; d * d <= n; d++)
        if (n % d == 0)
            return 0;
    return 1;
}

int main()
{
    int count = 0;
    for (int n = 1; n <= 50; n++) {
        if (!is_prime(n))
            continue;
        print(n);
        count++;
    }
    print("total primes:", count);
    return 0;
}
