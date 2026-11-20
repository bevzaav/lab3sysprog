int x = 100;

int main()
{
    int x = 1;
    print("outer x =", x);
    {
        int x = 2;
        print("inner x =", x);
        x += 10;
        print("inner x after += 10 =", x);
    }
    print("outer x again =", x);
    int i = 0;
    do {
        i++;
        if (i == 2)
            continue;
        if (i > 4)
            break;
        print("do-while i =", i);
    } while (i < 10);
    for (int k = 0; k < 6; k++) {
        if (k % 2 == 1)
            continue;
        print("for k =", k);
    }
    int j = i++;
    int m = ++i;
    print("i =", i, "j =", j, "m =", m);
    return 0;
}
