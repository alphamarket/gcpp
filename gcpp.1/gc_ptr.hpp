#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP

#include <memory>

namespace gc {
#   define base_gc_ptr(T) std::shared_ptr<T>
    template<typename T>
    class gc_ptr : public base_gc_ptr(T) {
    public:
#       include "inc/gc_ptr.ctor.inc"
#       include "inc/gc_ptr.opr.inc"
    };
    template <typename T>
    gc_ptr<T> ref_cast(T* p) { return gc_ptr<T>(p, [](T*)->void{}); }
#   undef base_gc_ptr
}


#endif // HDR__GC_PTR_HPP
