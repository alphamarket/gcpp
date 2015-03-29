#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP

#include <memory>

namespace gc {
    template<typename T>
    class gc_ptr : public std::shared_ptr<void> {
        typedef std::shared_ptr<void> base;
    public:
        void reset() { cout<<"RESET"; base::reset(); }

        void reset(T* p) {
            cout<<"RESET";
            base::reset(p);
        }

        void swap(gc_ptr& p) {
        cout<<"SWAPED"<<endl;
            base::swap(p);
        }

        T* get() const { return static_cast<T*>(base::get()); }
#       include "inc/gc_ptr.ctor.inc"
#       include "inc/gc_ptr.opr.inc"
    };
    template <typename T>
    gc_ptr<T> ref_cast(T* p) { return gc_ptr<T>(p, [](T*)->void{}); }
#   undef base_gc_ptr
}


#endif // HDR__GC_PTR_HPP
