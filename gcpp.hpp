#ifndef CPPGC_HPP
#define CPPGC_HPP

#include "gcpp/gc.hpp"

#define gc_new          new(::GC)
#define gc_collect()    gc::collect()

#endif
