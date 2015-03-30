#ifndef HDR__GC_HPP__
#define HDR__GC_HPP__

#if __cplusplus <= 199711L
#   error "This cannot be use for under C++11 version."
#endif

#include <memory>
#include <string>
#include <vector>
#include <ostream>
#include <algorithm>
#include "gcafx.hpp"
#include "flags.hpp"
#include "gc_ptr.hpp"
#include "typedefs.hpp"

void* operator new(size_t s, const ::GC&) {
  return ::operator new(s);
}
void* operator new(size_t s, const ::GC&, ostream& o, std::string file, unsigned line) {
    o<<"[new] "<<file<<": "<<line<<" ";
    return ::operator new(s);
}


#endif
