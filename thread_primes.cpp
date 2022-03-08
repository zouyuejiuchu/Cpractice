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
#include <atomic>
#include <unistd.h>

using namespace std;

int ans = 0;
void f(int priority) { 
    __sync_fetch_and_add(&ans, priority);
    return ;
}

int main() {
    zhang::thread_pool tp(5);
    tp.start();
    for (int i = 1; i <= 100; i++) {
        if (i == 10) tp.add_threads(10);
        if (i == 70) tp.minus_threads(14);
        tp.add_one_task(i, f, i);
    }
    tp.log();
    tp.stop_until_empty();
    cout << ans << endl;
    return 0;
}
