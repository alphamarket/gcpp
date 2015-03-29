#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <algorithm>
#include "gcafx.hpp"
namespace gc {

    template<typename T>
    class gc_weak_ptr;
#define ptoi(p) reinterpret_cast<std::intptr_t>(p)
    typedef std::pair<std::uintptr_t, gc_weak_ptr> map_elem;
    typedef std::vector<map_elem> gc_map;
    static gc_map map;
    template<typename T>
    class gc_ptr : public std::shared_ptr<void> {
        typedef std::shared_ptr<void>   base;
        typedef gc_ptr                  self;
        template<typename _T> friend gc_ptr<_T> ref_cast(_T*);
        template<typename _T> friend class gc_node;
        template<typename _T>  friend class gc_weak_ptr;
    protected:
        static void __prevent_delete(__unused T*) { cout<<"[DELX]"; }
        bool is_ref_casted = false;
    public:
        T* get() const { return static_cast<T*>(base::get()); }
        template<typename _T, typename _Delete>
        gc_ptr(_T* p, _Delete d) : base(p, d) {
            cout<<"CTOR: "<<p<<" "<<typeid(p).name()<<" "<<this->use_count()<<endl;
            map.push_back(std::make_pair<std::uintptr_t, gc_weak_ptr>(ptoi(p), *this));
        }

#       include "inc/gc_ptr.ctor.inc"
#       include "inc/gc_ptr.opr.inc"
    };
    template<typename T>
    class gc_weak_ptr  : public std::weak_ptr<void> {
        bool is_ref_casted = false;
    public:
        explicit gc_weak_ptr(const gc_ptr<T>& gp) : std::weak_ptr<void>(gp), is_ref_casted(gp.is_ref_casted)
        {}
    };
    template <typename T>
    gc_ptr<T> ref_cast(T* p) { auto x = gc_ptr<T>(p, gc::gc_ptr<T>::__prevent_delete); x.is_ref_casted = true; return x; }

    template <typename T>
    class gc_node {
        void*       _address;
        size_t      _size;
        gc_ptr<T>*  _val;
    public:
        gc_node(gc_ptr<T>* const gp) {
            this->_address = gp->get();
            this->_val = gp;
        }
        weak_ptr<T> getVal()        const { this->_val; }
        const void* getAddress()    const { return this->_address; }
    };
}
#endif // HDR__GC_PTR_HPP
