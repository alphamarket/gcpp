#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include "gcafx.hpp"
namespace gc {
#   define ptoi(p) reinterpret_cast<std::intptr_t>(p)
#   define where typename = typename
#   define deleted_signature(_T) void(*)(_T*)
    using namespace std;
    /**
     * class gc_ptr decl. this class is to manage pointers to objects
     * so it is not logical to accept pointers as input
     */
    template<typename T, where
        std::enable_if<
            !std::is_pointer<T>::value>::type>
    class gc_ptr                                                    : public std::shared_ptr<void> {
        typedef std::shared_ptr<void>   base;
        typedef gc_ptr                  self;
    protected:
        /**
         * the containing data
         */
        void*           _data = nullptr;
        /**
         * The possible events enum
         */
        typedef enum    EVENT { E_CTOR, E_DTOR, E_DELETE } EVENT;
        /**
         * The do not delete flag for stop deletion
         * at the end of contained pointer's life
         */
        template<typename _T = T, where
            std::enable_if<
                std::is_convertible<T, _T>::value>::type>
        static void dont_delete(T* p)
        { _event(E_DELETE, p); }
        /**
         * for general pointer delete constructor [ The return sorce of other ctor ]
         */
        template<typename _T, typename _Delete, where
            std::enable_if<
                std::is_convertible<_T, T>::value>::type>
        gc_ptr(_T* p, _Delete d)
            : base(p, d)
        { assert(*std::get_deleter<deleted_signature(_T)>(*this) == d); }
        /**
         * @brief event operator
         * @param e the event
         * @param p
         */
        static void _event(EVENT e, __unused const void* const p = nullptr) {
            switch(e) {
                case E_CTOR:
                    cout<<"[CTOR: "<<((self*)p)->get()<<"]"<<endl;
                    break;
                case E_DTOR:
                    cout<<"[DTOR: "<<((self*)p)->get()<<"]"<<endl;
                    break;
                case E_DELETE:
                    cout<<"[DELX: "<<p<<"]"<<endl;
                    break;
                default:
                    throw std::invalid_argument("Invalid event passed!");
            }
         }
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
        { _event(E_DTOR, this); }
        /**
         * for empty ptr
         */
        gc_ptr()
            : self(static_cast<T*>(nullptr), self::dont_delete)
        { _event(E_CTOR, this); }
        /**
         * for copy assignments [ only for convertable data types ]
         */
        template<typename _T, where
            std::enable_if<
                std::is_convertible<_T, T>::value>::type>
        gc_ptr(const gc_ptr<_T>& gp)
            : self(gp.get(), self::dont_delete)
        { _event(E_CTOR, this); }
        /**
         * for general pointer assignment [only on no<-conversion types between the class' T and the input _T]
         */
        template<typename _T, where
            std::enable_if<
                !std::is_reference<_T>::value ||
                std::is_same<T, _T>::value>::type>
        gc_ptr(_T* p)
            : self(p, self::dont_delete)
        { _event(E_CTOR, this); }
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
            : self(const_cast<_Tin*>(std::addressof(p)), self::dont_delete)
        { _event(E_CTOR, this); static_assert(std::is_pointer<_Tin>::value, "Cannot assign stack varibales as managed pointers!"); }
        /**
         * for base <- derived assignments [only for derived types, does not accept the same type]
         */
        template<typename D, typename B = T, where
            std::enable_if<
                !std::is_same<B, D>::value &&
                std::is_base_of<B, D>::value>::type>
        gc_ptr(D* p)
            : self(dynamic_cast<B*>(p), self::dont_delete)
        { _event(E_CTOR, this); }
        /**
         * assignment oprator
         */
        template<typename _T, where
            std::enable_if<
                std::is_convertible<_T, T>::value>::type>
        inline self& operator =(const gc_ptr<_T>& gp) {
            if(auto del_p = std::get_deleter<deleted_signature(T)>(gp))
                *this = self(gp.get<T>(), *del_p);
            else
                *this = self(gp.get<T>());
            return *this;
        }
    };
#   undef deleted_signature
#   undef where
#   undef ptoi
}
#endif // HDR__GC_PTR_HPP
