#pragma once
#include "iostream"
#include "singleton.h"

// exception
class Exception : public std::exception {
public:
    Exception(const std::string& str) : exception_str(str) {}

    const char * what () const throw () {
        return exception_str.c_str();
    }

private:
    std::string exception_str;
};

