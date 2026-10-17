#pragma once
#include <cstdio>
#include <cstdlib>
#define CHECK(value) do { if(!(value)) {std::fprintf(stderr,"Check failed at %s:%d: %s\n",__FILE__,__LINE__,#value);std::fflush(stderr);std::exit(1);} } while(false)
