#include "action_table.hh"
#include <dlfcn.h>
#include <memory>
#include <iostream>

External_AT_Wrapper::External_AT_Wrapper(std::string so_file) {
    handle = dlopen(so_file.c_str(), RTLD_LAZY);
    if (!handle) {
        throw std::runtime_error("Failed to load shared object" + std::string(dlerror()));
    }
}

External_AT_Wrapper::~External_AT_Wrapper() {
    if (handle) {
        dlclose(handle);
    }
}

std::unique_ptr<Base_Action_Table, destroy_action_table_t> External_AT_Wrapper::create_action_table() {
    auto create = reinterpret_cast<create_action_table_t>(dlsym(handle, "create"));
    auto destroy = reinterpret_cast<destroy_action_table_t>(dlsym(handle, "destroy"));
    if (!create || !destroy) {
        throw std::runtime_error("Failed to load create/destroy symbols from plugin");
    }
    return std::unique_ptr<Base_Action_Table, destroy_action_table_t>(create(), destroy);
}
