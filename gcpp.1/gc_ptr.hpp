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
    typedef struct gc_weak_ptr_str {
        std::weak_ptr<void> wp;
        bool is_ref_casted;
        template<typename T>
        gc_weak_ptr_str(const std::shared_ptr<T>& gp, bool is_ref_casted = false) {
            wp = gp;
            this->is_ref_casted = is_ref_casted;
        }
    } gc_weak_ptr_str;
#define ptoi(p) reinterpret_cast<std::intptr_t>(p)
    typedef std::unordered_multimap<void*, gc_weak_ptr_str> gc_map;
    static gc_map map;
    template<typename T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
    class gc_ptr : public std::shared_ptr<void> {
        typedef std::shared_ptr<void>   base;
        typedef gc_ptr                  self;
        template<typename _T>
            friend gc_ptr<_T> ref_cast(_T*);
        template<typename _T>
            friend class gc_node;
        template<typename _T>
            friend class gc_weak_ptr;
        friend gc_weak_ptr_str;
    protected:
        static void __prevent_delete(__unused T*) { cout<<"[DELX]"; }
        bool is_ref_casted = false;
    public:
        T* get() const { return static_cast<T*>(base::get()); }
        /**
         * for empty ptr
         */
        gc_ptr() : base() {}
        /**
         * for general pointer assignment [only on no-conversion types between the class' T and the input _T]
         */
        template<typename _T, typename = typename std::enable_if<std::is_same<T, _T>::value>::type>
        gc_ptr(_T* p) : base(p) { }
        /**
         * for general pointer delete constructor
         */
        template<typename _T, typename _Delete>
        gc_ptr(_T* p, _Delete d) : base(p, d) {
            assert(*std::get_deleter<void(*)(_T*)>(*this) == d);
        }
        ~gc_ptr() { cout<<std::boolalpha<<"("<<this->is_ref_casted<<")"<<endl; }
        /**
         * for stack var assignments
         */
        template<typename _Tin, typename = typename std::enable_if<!std::is_pointer<_Tin>::value>::type>
        gc_ptr(const _Tin& p) : self(const_cast<_Tin*>(std::addressof(p))) {
            static_assert(std::is_pointer<_Tin>::value, "Cannot assign stack varibales as managed pointers!");
        }
        /**
         * for base <- derived assignments [only for derived types, does not accept the same type]
         */
        template<typename D, typename B = T,
            typename = typename std::enable_if<!std::is_same<B, D>::value && std::is_base_of<B, D>::value>::type>
        gc_ptr(D* p): self(dynamic_cast<B*>(p), self::__prevent_delete) { }


#       include "inc/gc_ptr.ctor.inc"
#       include "inc/gc_ptr.opr.inc"
    };
    template <typename T>
    gc_ptr<T> ref_cast(T* p) { auto x = gc_ptr<T>(p, gc::gc_ptr<T>::__prevent_delete); x.is_ref_casted = true; return x; }

    template<typename T>
    class gc_weak_ptr : public std::weak_ptr<void> {
        typedef std::weak_ptr<void> base;
        typedef gc_weak_ptr self;
        bool is_ref_casted = false;
    public:
        constexpr gc_weak_ptr() noexcept : base() {}

          template<typename _Tp1, typename = typename
               std::enable_if<std::is_convertible<_Tp1*, T*>::value>::type>
        gc_weak_ptr(const std::weak_ptr<_Tp1>& __r) noexcept
        : base(__r) { }

          template<typename _Tp1, typename = typename
               std::enable_if<std::is_convertible<_Tp1*, T*>::value>::type>
        gc_weak_ptr(const std::shared_ptr<_Tp1>& __r) noexcept
        : base(__r) { }
        template<typename _T>
        gc_weak_ptr(const gc_weak_ptr<_T> &gwp) : is_ref_casted(gwp.is_ref_casted) {
            this->swap(gwp);
        }
        gc_weak_ptr(const gc_weak_ptr<T> &gwp) : is_ref_casted(gwp.is_ref_casted) {
            this->swap(gwp);
        }

        explicit gc_weak_ptr(const gc_ptr<T>& gp)
            : base(static_cast<std::shared_ptr<T>>(gp)), is_ref_casted(gp.is_ref_casted)
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
