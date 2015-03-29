#ifndef HDR__GC_HPP__
#define HDR__GC_HPP__

#if __cplusplus <= 199711L
#   error "This cannot be use for under C++11 version."
#endif

#include <memory>
#include <vector>
#include <algorithm>
#include "gcafx.hpp"
#include "flags.hpp"
#include "gc_ptr.hpp"
#include "typedefs.hpp"

void* operator new(size_t s, const ::GC&)
{
  return ::operator new(s);
}

namespace gc {
}


#endif
