#include "cp_buffer.hh"

#define __malloc_struct__(s, z)          \
    (struct s*)malloc(z*sizeof(struct s))

using namespace std;

CP_Buffer::CP_Buffer(size_t sz) {
    buffer = __malloc_struct__(datapoint, sz);
    size = sz;
    rdptr = 0;
    wrptr = 0;
    abort = false;
}
CP_Buffer::~CP_Buffer() {
    free(buffer);
    size = 0;
    rdptr = 0;
    wrptr = 0;
}

size_t CP_Buffer::get(struct datapoint* output) {
    unique_lock<mutex> lock(mtx);
    while((rdptr == wrptr) && !abort) {
        cv.wait(lock);
    }

    size_t i = 0;
    while((rdptr != wrptr) && !abort) {
        output[i++] = buffer[rdptr];
        rdptr = (rdptr + 1) % size;
    }

    return i;
}

bool CP_Buffer::put(struct datapoint data) {
    bool ret = true;
    unique_lock<mutex> lock(mtx);

    buffer[wrptr] = data;
    wrptr = (wrptr + 1) % size;

    if(wrptr == rdptr) {
        rdptr = (rdptr + 1) % size;
        ret = false;
    }

    lock.unlock();
    cv.notify_one();

    return ret;
}

void CP_Buffer::stop() {
    {
        unique_lock<mutex> lock(mtx);
        abort = true;
    }
    cv.notify_all();
}

void CP_Buffer::restart() {
    unique_lock<mutex> lock(mtx);
    abort = false;
}