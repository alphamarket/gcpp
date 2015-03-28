#ifndef GCPP_HPP
#define GCPP_HPP

#undef gc_new
#undef gc_collect

#if !defined(ENABLE_GCPP_V0) &&\
    !defined(ENABLE_GCPP_V1)
#   define ENABLE_GCPP_DEFAULT
#endif

#ifdef ENABLE_GCPP_DEFAULT
#   define ENABLE_GCPP_V1
#endif

#if defined(ENABLE_GCPP_V0)
/*
 * Enable gcpp version 0
 */
#   include "gcpp.0/gc.hpp"

#   define gc_new          new(::GC)
#   define gc_collect()    gc::collect()
#elif defined(ENABLE_GCPP_V1)

#define gc_new
#define gc_collect()
#endif

#if !defined(gc_new) || !defined(gc_collect)
#   error `gc_new` and `gc_collect` both need to be defined by the end of this file
#endif

#endif
