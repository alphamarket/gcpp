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
#   define ptoi(p) reinterpret_cast<std::intptr_Tin>(p)
#   define where typename = typename
#   define deleted_signature(_Tin) void(*)(_Tin*)
#   define should_dynamic_cast(BASE, DRIVED) !std::is_same<BASE, DRIVED>::value && std::is_class<BASE>::value && std::is_base_of<BASE, DRIVED>::value
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
        bool            is_stack = false;
        /**
         * the containing data
         */
        void*           _data = nullptr;
        /**
         * The possible events enum
         */
        enum    class EVENT { E_CTOR, E_DTOR, E_MOVE, E_DELETE };
        /**
         * The do not delete flag for stop deletion
         * at the end of contained pointer's life
         */
        template<typename _Tin = T, where
            std::enable_if<
                std::is_convertible<T, _Tin>::value>::type>
        static void dont_delete(T* p)
        { _event(EVENT::E_DELETE, p); }
        /**
         * for general pointer delete constructor [ The return sorce of other ctor ]
         */
        template<typename _Tin, typename _Delete, where
            std::enable_if<
                std::is_pointer<_Delete>::value &&
                std::is_convertible<_Tin, T>::value>::type>
        inline gc_ptr(_Tin* p, _Delete d)
            : base(p, d)
        { assert(*std::get_deleter<deleted_signature(_Tin)>(*this) == d); }
        /**
         * @brief event operator
         * @param e the event
         * @param p
         */
        static void _event(EVENT e, __unused const void* const p = nullptr) {
            switch(e) {
                case EVENT::E_CTOR:
                    cout<<"[CTOR: "<<((self*)p)->get()<<"]";
                    break;
                case EVENT::E_DTOR:
                    cout<<"[DTOR: "<<((self*)p)->get()<<"]";
                    break;
                case EVENT::E_MOVE:
                    cout<<"[MOVE: "<<((self*)p)->get()<<"]";
                    break;
                case EVENT::E_DELETE:
                    cout<<"[DELX: "<<p<<"]"<<endl;
                    return;
                default:
                    throw std::invalid_argument("Invalid event passed!");
            }
            if(((self*)p)->is_stack)
                cout<<"(ON STACK)";
            cout<<endl;
         }
    public:
        /**
         * get the internal pointer with a cast
         */
        template<typename _Tout = T, where
            std::enable_if<
                std::is_convertible<T, _Tout>::value>::type>
        inline _Tout* get() const
        {
            if(should_dynamic_cast(_Tout, T))
                return dynamic_cast<_Tout*>(base::get());
            return static_cast<_Tout*>(base::get());
        }
        /**
         * the dtor
         */
        inline ~gc_ptr()
        { _event(EVENT::E_DTOR, this); }
        /**
         * for empty ptr
         */
        inline gc_ptr()
            : self(static_cast<T*>(nullptr), self::dont_delete)
        { _event(EVENT::E_CTOR, this); }
        /**
         * for copy assignments [ only for convertable data types ]
         */
        template<typename _Tin, where
            std::enable_if<
                std::is_convertible<_Tin, T>::value>::type>
        inline gc_ptr(const gc_ptr<_Tin>& gp)
            : self(gp.get(), *std::get_deleter<deleted_signature(T)>(gp))
        { _event(EVENT::E_CTOR, this); }
        /**
         * for move assignments [ only for convertable data types ]
         */
        template<typename _Tin, where
            std::enable_if<
                std::is_convertible<_Tin, T>::value>::type>
        inline gc_ptr(const gc_ptr<_Tin>&& gp)
        {  *this = std::move(gp); _event(EVENT::E_MOVE, this); }
        /**
         * for general pointer assignment [only on no<-conversion types between the class' T and the input _Tin]
         */
        template<typename _Tin, where
            std::enable_if<
                std::is_same<T, _Tin>::value>::type>
        inline gc_ptr(_Tin* p, bool stack_alloced = false)
            : self(p, self::dont_delete)
        { this->is_stack = stack_alloced; _event(EVENT::E_CTOR, this); }
        /**
         * for stack var assignments
         */
        template<typename _Tin, where
            std::enable_if<
                !std::is_same<_Tin, self>::value &&
                std::is_convertible<_Tin, T>::value &&
                !std::is_pointer<_Tin>::value>::type>   // this cond. made and ~this cond. make in the
                                                        // below assertion to make sure we stop the stack setting:)
        inline gc_ptr(const _Tin& p)
            : self(const_cast<_Tin*>(std::addressof(p)), self::dont_delete)
        { _event(EVENT::E_CTOR, this); static_assert(std::is_pointer<_Tin>::value, "Cannot assign stack varibales as managed pointers!"); }
        /**
         * for base <- derived assignments [only for derived types, does not accept the same type]
         */
        template<typename D, typename B = T, where
            std::enable_if<
                std::is_class<B>::value &&
                !std::is_same<B, D>::value &&
                std::is_base_of<B, D>::value>::type>
        inline gc_ptr(D* p, bool stack_alloced = false)
            : self(dynamic_cast<B*>(p), self::dont_delete)
        { this->is_stack = stack_alloced; _event(EVENT::E_CTOR, this); }
        /**
         * assignment oprator
         */
        template<typename _Tin, where
            std::enable_if<
                std::is_convertible<_Tin, T>::value>::type>
        inline self& operator =(const gc_ptr<_Tin>& gp) {
            if(auto del_p = std::get_deleter<deleted_signature(T)>(gp))
                *this = self(gp.get<T>(), *del_p);
            else
                *this = self(gp.get<T>());
            return *this;
        }
    };
    /**
     * converts a reference pointer to a stack varibale to gc_ptr
     */
    template<typename _Tout, typename _Tin, where
        std::enable_if<
            std::is_convertible<_Tin, _Tout>::value>::type>
    inline gc::gc_ptr<_Tout> ref2ptr(_Tin* ref)
    {
        if(should_dynamic_cast(_Tout, _Tin))
            return gc::gc_ptr<_Tout>(dynamic_cast<_Tout*>(ref), true);
        return gc::gc_ptr<_Tout>(static_cast<_Tout*>(ref), true);
    }
#   undef should_dynamic_cast
#   undef deleted_signature
#   undef where
#   undef ptoi
}
#endif // HDR__GC_PTR_HPP
