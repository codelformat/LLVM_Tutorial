
#ifndef Logger_h
#define Logger_h

#include <iostream>
#include <string>

class ErrorLogMessage : public std::basic_ostringstream<char>
{
public:
    ~ErrorLogMessage()
    {
        std::cerr << "Fatal error: " << str().c_str() << std::endl;
    }
};

#define DIE ErrorLogMessage()

#endif