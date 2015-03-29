#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <unordered_map>
#include "gcafx.hpp"
namespace gc {
    using namespace std;
    template<typename T>
    class gc_weak_ptr;
#define ptoi(p) reinterpret_cast<std::intptr_t>(p)
//    typedef std::unordered_multimap<void*, gc_weak_ptr<void>> gc_map;
//    static gc_map map;
    template<typename T>
    class gc_ptr : public std::shared_ptr<void> {
        typedef std::shared_ptr<void>   base;
        typedef gc_ptr                  self;
        template<typename _T, typename _U>
            friend gc_ptr<_T> ref_cast(_U*);
        template<typename _T>
            friend class gc_node;
        template<typename _T>
            friend class gc_weak_ptr;
    protected:
        static void __prevent_delete(__unused T*) { cout<<"[DELX]"; }
        bool is_ref_casted = false;
    public:
        T* get() const { return static_cast<T*>(base::get()); }
        template<typename _T, typename _Delete>
        gc_ptr(_T* p, _Delete d) : base(p, d) {
            cout<<"CTOR: "<<p<<" "<<typeid(p).name()<<" "<<this->use_count()<<endl;
//            map.push_back(std::make_pair<std::uintptr_t, gc_weak_ptr<void>>(ptoi(p), ));
        }
        ~gc_ptr() { cout<<std::boolalpha<<"("<<this->is_ref_casted<<")"<<endl; }
        template<typename _Tin, typename =
        typename std::enable_if<!std::is_pointer<_Tin>::value>::type>
        gc_ptr(const _Tin& p) : self(const_cast<_Tin*>(std::addressof(p))) {
            static_assert(std::is_pointer<_Tin>::value, "Cannot assign stack varibales as managed pointers!");
        }
        template<typename D, typename B = T, typename =
        typename std::enable_if<std::is_base_of<B, D>::value>::type>
        gc_ptr(D* p) : self(dynamic_cast<B*>(p), self::__prevent_delete){
            cout<<"D2B"<<endl;
        }


#       include "inc/gc_ptr.ctor.inc"
#       include "inc/gc_ptr.opr.inc"
    };
    template<typename T>
    class gc_weak_ptr  {
        bool is_ref_casted = false;
        std::weak_ptr<void> wptr;
    public:
        gc_weak_ptr() {}
        template<typename _T>
        gc_weak_ptr(const gc_weak_ptr<_T> &gwp) : is_ref_casted(gwp.is_ref_casted) {
            this->wptr.swap(gwp);
        }
        gc_weak_ptr(const gc_weak_ptr<T> &gwp) : is_ref_casted(gwp.is_ref_casted) {
            this->wptr.swap(gwp);
        }

        explicit gc_weak_ptr(const gc_ptr<T>& gp) : wptr(gp), is_ref_casted(gp.is_ref_casted)
        { }

        const std::weak_ptr<void>& get_ptr() const { return this->wptr; }
    };

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
