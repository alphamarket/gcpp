#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <unordered_map>
#include "gcafx.hpp"
namespace gc {
#   define ptoi(p) reinterpret_cast<std::intptr_t>(p)
#   define where typename = typename
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

    typedef std::unordered_multimap<void*, gc_weak_ptr_str> gc_map;
    static gc_map map;
    template<typename T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
    class gc_ptr : public std::shared_ptr<void> {
        typedef std::shared_ptr<void>   base;
#       define deleted_signature(_T) void(*)(_T*)
        typedef gc_ptr                  self;
        template<typename _T>
            friend gc_ptr<_T> ref_cast(_T*);
        template<typename _T>
            friend class gc_node;
        template<typename _T>
            friend class gc_weak_ptr;
        friend struct gc_weak_ptr_str;
    protected:
        static void dont_delete(T*) { cout<<"[DELX]"; }
        void*   _data = NULL;
        /**
         * for general pointer delete constructor
         */
        template<typename _T, typename _Delete, where
            std::enable_if<
                std::is_convertible<_T, T>::value>::type>
        gc_ptr(_T* p, _Delete d)
            : base(p, d)
        { assert(*std::get_deleter<deleted_signature(_T)>(*this) == d); }
    public:
        /**
         * get the internal pointer with a cast
         */
        template<typename _T = T, where
            std::enable_if<
                std::is_convertible<T, _T>::value>::type>
        _T* get() const
        { return static_cast<_T*>(base::get()); }
        /**
         * the dtor
         */
        ~gc_ptr()
        { }
        /**
         * for empty ptr
         */
        gc_ptr()
            : base(nullptr, self::dont_delete)
        { }
        /**
         * for copy assignments [ only for convertable data types ]
         */
        template<typename _T, where
            std::enable_if<
                std::is_convertible<_T, T>::value>::type>
        gc_ptr(const gc_ptr<_T>& gp)
            : self(gp.get())
        { }
        /**
         * for general pointer assignment [only on no<-conversion types between the class' T and the input _T]
         */
        template<typename _T, where
            std::enable_if<
                !std::is_reference<_T>::value ||
                std::is_same<T, _T>::value>::type>
        gc_ptr(_T* p)
            : self(p, self::dont_delete)
        { }
        /**
         * for stack var assignments
         */
        template<typename _Tin, where
            std::enable_if<
                !std::is_same<_Tin, self>::value &&
                std::is_convertible<_Tin, T>::value &&
                !std::is_pointer<_Tin>::value>::type>   // this cond. made and ~this cond. make in the
                                                        // below assertion to make sure we stop the stack setting:)
        gc_ptr(const _Tin& p)
            : self(const_cast<_Tin*>(std::addressof(p)))
        { static_assert(std::is_pointer<_Tin>::value, "Cannot assign stack varibales as managed pointers!"); }
        /**
         * for base <- derived assignments [only for derived types, does not accept the same type]
         */
        template<typename D, typename B = T, where
            std::enable_if<
                !std::is_same<B, D>::value &&
                std::is_base_of<B, D>::value>::type>
        gc_ptr(D* p)
            : self(dynamic_cast<B*>(p), self::dont_delete)
        { }
        /**
         * assignment oprator
         */
        template<typename _T, where
            std::enable_if<
                std::is_convertible<_T, T>::value>::type>
        inline self& operator =(const gc_ptr<_T>& gp) {
            if(auto del_p = std::get_deleter<deleted_signature(T)>(gp))
                this->reset(gp.get<T>(), *del_p);
            else
                this->reset(gp.get<T>());
            return *this;
        }
    };
#   undef where
#   undef ptoi
}
#endif // HDR__GC_PTR_HPP
