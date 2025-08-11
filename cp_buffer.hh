#ifndef CP_BUFFER_H
#define CP_BUFFER_H

#include <mutex>
#include <string>
#include <memory>
#include <condition_variable>

#include <time.h>


struct datapoint {
    uint32_t time;          // ms since 00:00 (midnight)
    uint32_t payload;
    uint8_t index;
} __attribute__((packed));

class Base_Buffer {
public:
    virtual size_t get(struct datapoint* output) = 0;
    virtual size_t size() = 0;
    virtual void stop() = 0;
    virtual void restart() = 0;

    Base_Buffer() = default;
    ~Base_Buffer() = default;
};


class CP_Buffer : public Base_Buffer {
    std::condition_variable cv;
    std::mutex mtx;

    std::unique_ptr<datapoint[]> buffer;
    size_t sz;
    size_t rdptr;
    size_t wrptr;

    bool abort;
public:
    CP_Buffer(size_t sz);
    
    size_t get(struct datapoint* output) override;
    bool put(struct datapoint data);
    size_t size() override {return sz;}
    void stop() override;
    void restart() override;
};

typedef uint8_t error_flag_t;

class FIFO_Buffer : public Base_Buffer {
    enum Error_Flag : error_flag_t {
        open_error,
        poll_error,
        read_error,

        n_flags,        // number of flags
    };

    int fd;
    std::unique_ptr<datapoint[]> buffer;
    size_t sz;
    size_t rdptr;
    size_t wrptr;

    bool abort;
    error_flag_t eflag;

public:
    FIFO_Buffer(size_t sz, std::string fifo_path);
    
    size_t get(struct datapoint* output) override;
    size_t size() override {return sz;}
    void stop() override {abort = true;}
    void restart() override {abort = false;}

    bool check_error(error_flag_t* e) {
        *e = eflag;
        return eflag == 0;
    }

    void clear_error() {eflag ^= eflag;}
};

#endif