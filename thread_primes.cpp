#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <map>
#include <cmath>
#include <queue>
#include "thread_pool.h"
using namespace std;

int is_prime(int x) {
    if (x <= 1) return 0;
    for (int i = 2; i * i <= x; i++) {
        if (x % i == 0) return 0;
    }
    return 1;
}

void prime_cnt(int &ans, int l, int r) {
    int cnt = 0;
    for (int i = l; i <= r; i++) {
        cnt += is_prime(i);
    }
    __sync_fetch_and_add(&ans, cnt);
    return ;
}

void f(int priority) {
    cout << priority << endl;
}

int main() {
    int cnt = 0;
    zhang::thread_pool tp(5);
    tp.start();
    //for (int i = 1; i <= 100; i++) {
    //    tp.add_one_task(i, prime_cnt, ref(cnt), (i - 1) * 100000 + 1, i * 100000);
    //}
    tp.add_one_task(1, f, 1);
    tp.add_one_task(2, f, 2);
    tp.add_one_task(3, f, 3);
    tp.add_one_task(4, f, 4);
    tp.stop_until_empty();
    cout << cnt << endl;
    return 0;
}
