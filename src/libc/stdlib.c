#include "stdlib.h"

void reverse(char s[]) {
    int i, j;
    char c;
    for (i = 0, j = 0; s[j] != 0; j++);
    j--;
    for (; i < j; i++, j--) {
        c = s[i]; s[i] = s[j]; s[j] = c;
    }
}

void itoa(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';
    reverse(str);
}

int atoi(const char* str) {
    int res = 0;
    int sign = 1;
    int idx = 0;

    if (str[0] == '-') {
        sign = -1;
        idx++;
    }
    for (; str[idx] != '\0'; ++idx) {
        if (str[idx] < '0' || str[idx] > '9') {
            break;
        }

        res = res * 10 + (str[idx] - '0');
    }

    return sign * res;
}