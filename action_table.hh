#ifndef ACTION_TABLE_H
#define ACTION_TABLE_H

#include <memory>
#include <cstring>
#include <string>

#include <unistd.h>


class Base_Action_Table {
public:
    virtual ~Base_Action_Table() = default;
    virtual bool call(uint8_t command, uint8_t* payload, size_t length) = 0;
};

class Process_Relay_AT : public Base_Action_Table {

    std::unique_ptr<int[]> fds;
    size_t sz;

 public:
    Process_Relay_AT(int* fd, size_t size) : sz(size) {
        fds = std::make_unique<int[]>(size);
        memcpy(fds.get(), fd, size * sizeof(int));
    }

    ~Process_Relay_AT() = default;

    bool call(uint8_t command, uint8_t* payload, size_t length) override {
        if (command == 0xFF) {      // set 0xFF to no op
            return true;
        }   

        if (command > sz) {
            return false;
        }

        size_t n = 0;
        do {
            ssize_t nwrite = write(fds[command], payload + n, length - n);
            if (nwrite == -1) {
                return false;
            }

            n += nwrite;
        } while (n < length);

        return n == length;
    }
};

using create_action_table_t = Base_Action_Table* (*)();
using destroy_action_table_t = void (*)(Base_Action_Table*);


class External_AT_Wrapper {
    void* handle;
public:
    External_AT_Wrapper(std::string so_file);
    ~External_AT_Wrapper();

    std::unique_ptr<Base_Action_Table, destroy_action_table_t> create_action_table();
};

#endif