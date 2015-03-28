#ifndef GCPP_HPP
#define GCPP_HPP

#ifndef ENABLE_GCPP_V0
/*
 * Enable gcpp version 0
 */

#   include "gcpp.old/gc.hpp"

#   define gc_new          new(::GC)
#   define gc_collect()    gc::collect()
#else
#endif

#endif
