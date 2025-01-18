#ifndef CP_BUFFER_H
#define CP_BUFFER_H

#include <mutex>
#include <string>
#include <memory>
#include <condition_variable>

#include <time.h>

struct datapoint {
    int data;
    struct timespec ts;
};

class CP_Buffer {
    std::condition_variable cv;
    std::mutex mtx;

    struct datapoint *buffer;
    size_t size;
    size_t rdptr;
    size_t wrptr;

    bool abort;
public:
    CP_Buffer(size_t sz);
    ~CP_Buffer();

    size_t get(struct datapoint* output);
    bool put(struct datapoint data);

    size_t sz() {return size;}

    void stop();

    void restart();

};

#endif