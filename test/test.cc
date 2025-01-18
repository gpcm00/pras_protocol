#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cstdio>

#include <algorithm>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <thread>

#include "protocol.hh"

#define BUFLEN		64
#define LEN(arr) 	(sizeof(arr)/sizeof(arr[0]))

using namespace std;

#define MAX_NSEC             (999999999UL)
#define __milliseconds__(ms) (ulong)(ms*1000000UL)

inline ulong get_ms(unsigned long nsec) {
    return nsec / 1000000UL;
}

CP_Buffer dut(10);

bool done = false;

inline void inc_msec(struct timespec& ts, ulong ms) {
    ts.tv_nsec += __milliseconds__(ms);
    if(ts.tv_nsec > MAX_NSEC) {
        ts.tv_sec++;
        ts.tv_nsec -= MAX_NSEC;
    }
}

void producer() {
    struct timespec start;
    struct timespec sample;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int i = 0;
    // for(int i = 0; i < 100; i++) {
    while(!done) {
        clock_gettime(CLOCK_MONOTONIC, &sample);
            
        struct datapoint dp;
        dp.ts.tv_sec = sample.tv_sec - start.tv_sec;
        dp.ts.tv_nsec = sample.tv_nsec - start.tv_nsec;
        dp.data = 2*i-10;
        // printf("%d, %d, %ld\n", dp.data, dp.ts.tv_sec, dp.ts.tv_nsec);
        dut.put(dp);
        inc_msec(sample, 200);
        int r = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &sample, NULL);
        if(r != 0)
            printf("nsleep: %s\n", strerror(r));
        i++;
    }
    printf("Producer done!\n");
    // done = true;
    // dut.terminate();
}

void consumer() {
    struct datapoint dp[10];
    memset(&dp, 0, sizeof(dp));
    while(!done) {
        size_t len = dut.get(dp);
        for(size_t i = 0; i < len; i++) {
            ulong ms = get_ms(dp[i].ts.tv_nsec);
            printf("%lds %ldms - %d\n", dp[i].ts.tv_sec, ms, dp[i].data);
        }
    }
}

int main() {
    Protocol prot("80", dut);
    prot.change_duration(1);
    thread net(ref(prot));
    printf("waiting at %d\n", prot.current_state());
    while(true) {
        volatile protocol_state_t s = prot.current_state();
        if(s != listening) break;
        // printf("%d", s);
        continue;
    }
    printf("initializing thread\n");
    thread p(producer);
    while(true) {
        volatile protocol_state_t s = prot.current_state();
        if(s == listening) break;
        // printf("%s\n", statename(s).c_str());
        continue;
    }
    // done = true;
    p.join();
    // prot.stop();

    net.join();

    printf("done\n");

    return 0;
}
