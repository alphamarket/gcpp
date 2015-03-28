#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP

#include <memory>

namespace gc {
#   define base_gc_ptr(T) std::shared_ptr<T>
    template<typename T>
    class gc_ptr : public base_gc_ptr(T) {
    public:
        gc_ptr<T>& as_ref() {
//            *this = base_gc_ptr(T)(this, [](T*) { });
            return *this;
        }

#       include "inc/gc_ptr.ctor.inc"
#       include "inc/gc_ptr.opr.inc"
    };
    template<typename T>
    gc_ptr<T>& gc_ref(T* p) {
    #define __prevent_del [](T*)->void{cout<<"DELETE PREVENTED"<<endl;}
        return gc_ptr<T>(p, __prevent_del);
    #undef __prevent_del
    }

#   undef base_gc_ptr
}


#endif // HDR__GC_PTR_HPP
