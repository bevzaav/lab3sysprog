int steps(int n)
{
    int count = 0;
    while (n != 1) {
        if (n % 2 == 0)
            n /= 2;
        else
            n = 3 * n + 1;
        count++;
    }
    return count;
}

int main()
{
    int best = 0, best_n = 0;
    for (int n = 1; n < 30; n++) {
        int s = steps(n);
        if (s > best) {
            best = s;
            best_n = n;
        }
    }
    print("longest Collatz chain below 30: n =", best_n, ", steps =", best);
    return 0;
}
