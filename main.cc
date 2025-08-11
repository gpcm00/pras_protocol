#include <string>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <map>
#include <vector>
#include <algorithm>
#include <optional>
#include <thread>

#include <unistd.h>
#include <fcntl.h>

#include "protocol.hh"

using namespace std;
namespace fs = std::filesystem;

#define PR_ERR(fmt, ...)    fprintf(stderr, fmt, ## __VA_ARGS__)

pair<string, string> parse_line(const string& line) {
    size_t eq_pos = line.find('=');
    if (eq_pos == string::npos) {
        return make_pair("","");
    }

    string lhs = line.substr(0, eq_pos);
    lhs.erase(0, lhs.find_first_not_of(" \t\r\n"));
    lhs.erase(lhs.find_last_not_of(" \t\r\n") + 1);

    size_t first_quote = line.find('"', eq_pos);
    size_t second_quote = line.find('"', first_quote + 1);
    if (first_quote == std::string::npos || second_quote == std::string::npos) { 
        return make_pair("","");
    }

    string rhs = line.substr(first_quote + 1, second_quote - first_quote - 1);

    return make_pair(lhs, rhs);
}

bool running = true;


int main(int argc, char** argv) {
    if (argc != 2) {
        PR_ERR("Usage: %s <config_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fs::path config_file(argv[1]);

    if (!fs::exists(config_file)) {
        PR_ERR("Can't find config file: %s\n", config_file.string().c_str());
        exit(EXIT_FAILURE);
    }

    ifstream fs(config_file);

    if (!fs) {
        PR_ERR("Can't open file: %s\n", config_file.string().c_str());
        exit(EXIT_FAILURE);
    }

    string line;
    map<string,string> config;
    vector<string> config_set = {
        "PORT",
        "STREAM_PORT",
        "CA_CERT",
        "KEY",
        "CERTIFICATE",
        "PASSWORD",
        "DURATION",
        "FIFO",
    };

    size_t min_config = config_set.size();

    while (getline(fs, line)) {
        auto [lhs, rhs] = parse_line(line);
        config[lhs] = rhs;
        config_set.erase(remove(config_set.begin(), config_set.end(), lhs), config_set.end());
    }

    if (!config_set.empty()) {
        PR_ERR("Invalid configuration file!\n"
                "Include the following:\n");
        for (const auto& c : config_set) {
            PR_ERR("%s\n", c.c_str());
        }
        exit(EXIT_FAILURE);
    }

    size_t n_files = config.size() - min_config;

    unique_ptr<int[]> fds = make_unique<int[]>(n_files);
    for (size_t i = 0; i < n_files; i++) {
        auto it = config.find(to_string(i));
        if (it == config.end()) {
            PR_ERR("FIFOs must be in order zero-based index\n");
            exit(EXIT_FAILURE);
        }

        fds[i] = open(it->second.c_str(), O_RDWR | O_NONBLOCK);
        if (fds[i] == -1) {
            perror("open: ");
            exit(EXIT_FAILURE);
        }

        printf("[+] Routing %ld to %s with fd %d\n", i, it->second.c_str(), fds[i]);
    }

    Process_Relay_AT at(fds.get(), n_files);  
    FIFO_Buffer cp(10, config["FIFO"]);

    unsigned duration = static_cast<unsigned>(stoul(config["DURATION"]));
    Server_Protocol server(
        config["PORT"],
        config["STREAM_PORT"],
        cp, at,
        config["CA_CERT"],
        config["CERTIFICATE"],
        config["KEY"],
        config["PASSWORD"],
        duration
    );

    printf("[+] Server initialized\n");

    optional<thread> t = server.start();
    if (!t.has_value()) {
        PR_ERR("Failed to create thread\n");
        exit(EXIT_FAILURE);
    }

    while (server.is_running());

    if (t->joinable()) {
        t->join();
    }

    exit(EXIT_SUCCESS);
}