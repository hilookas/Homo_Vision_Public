#include "utils.h"

// 快速排序算法，对指针进行排序，使用自定义比较函数
void my_sort(bool (*cmp)(void *, void *), void *a[], int l, int r) {
    if (l >= r) return;
    int m, i, j;

    m = (l + r) / 2;
    void *t = a[l];
    a[l] = a[m];
    a[m] = t;

    i = l;
    j = r;
    void *k = a[i];
    while (i < j) {
        while (i < j && !cmp(a[j], k)) j--;
        if (i < j) {
            a[i] = a[j];
            i++;
        }
        while (i < j && cmp(a[i], k)) i++;
        if (i < j) {
            a[j] = a[i];
            j--;
        }
    }
    a[i] = k;
    my_sort(cmp, a, l, i - 1);
    my_sort(cmp, a, i + 1, r);
}