#include "cp_buffer.hh"

#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

#define BIT(n)  (1<<(n))

using namespace std;

static const uint8_t empty_data[sizeof(datapoint)] = {0};

static ssize_t doread(int fd, uint8_t* buf, size_t size) {
    size_t n = 0;
    do {
        ssize_t nread = read(fd, buf, size);
        if (nread >= 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }

            return nread | n;   // -1 will set everything, but 0 won't
        }

    } while (n < size);

    return n;
}

CP_Buffer::CP_Buffer(size_t sz) : sz(sz) {
    buffer = make_unique<datapoint[]>(sz);
    rdptr = 0;
    wrptr = 0;
    abort = false;
}

size_t CP_Buffer::get(struct datapoint* output) {
    unique_lock<mutex> lock(mtx);
    while((rdptr == wrptr) && !abort) {
        cv.wait(lock);
    }

    size_t i = 0;
    while((rdptr != wrptr) && !abort) {
        output[i++] = buffer[rdptr];
        rdptr = (rdptr + 1) % sz;
    }

    return i;
}

bool CP_Buffer::put(struct datapoint data) {
    bool ret = true;

    {
        unique_lock<mutex> lock(mtx);

        buffer[wrptr] = data;
        wrptr = (wrptr + 1) % sz;

        if(wrptr == rdptr) {
            rdptr = (rdptr + 1) % sz;
            ret = false;
        }
    }

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


FIFO_Buffer::FIFO_Buffer(size_t sz, string fifo_path) : sz(sz) {
    rdptr = 0;
    wrptr = 0;
    abort = false;

    fd = open(fifo_path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        eflag |= BIT(open_error);
    }
}

size_t FIFO_Buffer::get(struct datapoint* output) {
    struct pollfd pfd = {fd, POLLIN, 0};
    int ret = 0;
    while (ret == 0 && !abort) {
        ret = poll(&pfd, 1, 1000); // check abort flag every second
    }

    if (ret == -1 || !(pfd.revents & POLLIN) || abort) {
        if (ret == -1 || !(pfd.revents & POLLIN)) {
            eflag |= BIT(poll_error);
        }
        return 0;
    }

    uint8_t* buf = reinterpret_cast<uint8_t*>(output);
    ssize_t nread = doread(fd, buf, sz * sizeof(datapoint));
    if (nread <= 0) {
        if (nread == -1) {
            eflag |= BIT(read_error);
        }
        return 0;
    }

    return nread;
}