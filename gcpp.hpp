#ifndef GCPP_HPP
#define GCPP_HPP

#ifndef __cplusplus
#  error "Garbage collecting is only works for C++."
#endif


#undef gc_new
#undef gc_collect

#if !defined(ENABLE_GCPP_V0) &&\
    !defined(ENABLE_GCPP_V1)
#   define ENABLE_GCPP_DEFAULT
#endif

#ifdef ENABLE_GCPP_DEFAULT
#   define ENABLE_GCPP_V1
#endif

//#define ENABLE_GCPP_V0

#if defined(ENABLE_GCPP_V0)
/**
 * Enable gcpp version 0
 */
#   include "gcpp.0/gc.hpp"

#   define gcnew        new(::GC)
#   define gccollect()  gc::collect()
#elif defined(ENABLE_GCPP_V1)
/**
 * Enable gcpp version 1
 */
#   include "gcpp.1/gc.hpp"
#   ifdef GC_DEBUG
#       define gcnew    new(::GC(), cout, __FILE__, __LINE__)
#   else
#       define gcnew    new(::GC())
#   endif
#   define gccollect()
#endif

#if !defined(gcnew) || !defined(gccollect)
#   error "`gc_new` and `gc_collect` both need to be defined by the end of this file."
#endif

#endif
