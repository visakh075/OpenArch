#pragma once
#include <string>
struct Result{bool ok;std::string message;
static Result success(const std::string&m="OK"){return{true,m};}
static Result failure(const std::string&m){return{false,m};}};
