#pragma once
#include <string>

struct Result {
    bool ok;
    std::string message;

    static Result success(const std::string& msg = "OK") {
        return {true, msg};
    }
    static Result failure(const std::string& msg) {
        return {false, msg};
    }
};
