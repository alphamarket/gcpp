#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include "gcafx.hpp"

namespace gc {
    template<typename T>
    class gc_ptr : public std::shared_ptr<void> {
        typedef std::shared_ptr<void>   base;
        typedef gc_ptr                  self;
        template <typename _T> gc_ptr<_T> ref_cast(_T*);
    protected:
        static void __prevent_delete(__unused T*) { }
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
    gc_ptr<T> ref_cast(T* p) { return gc_ptr<T>(p, gc::gc_ptr<T>::__prevent_delete); }
#   undef base_gc_ptr
}


#endif // HDR__GC_PTR_HPP
