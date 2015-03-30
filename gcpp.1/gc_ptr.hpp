#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#ifdef GC_DEBUG
#   include <sstream>
#endif

#include "gcafx.hpp"
namespace gc {
#   define ptoi(p)                          reinterpret_cast<std::intptr_Tin>(p)
#   define where                            typename = typename
#   define deleted_signature(_Tin)          void(*)(_Tin*)
#   define can_cast(FROM, TO)               std::is_convertible<FROM, TO>::value
#   define can_dynamic_cast(BASE, DERIVED)  can_cast(DERIVED, BASE) && !std::is_same<BASE, DERIVED>::value && std::is_class<BASE>::value && !std::is_const<DERIVED>::value && std::is_base_of<BASE, DERIVED>::value
#   define can_static_cast(FROM, TO)        can_cast(FROM, TO)
    using namespace std;
    /**
     * class gc_ptr decl. this class is to manage pointers to objects
     * so it is not logical to accept pointers as input
     */
    template<typename T>
    class gc_ptr                                                    : public std::shared_ptr<void> {
        static_assert(!std::is_pointer<T>::value, "cannot accept pointers as type!");
        typedef std::shared_ptr<void>   base;
        typedef gc_ptr                  self;
#ifdef  GC_DEBUG
    public:
        static size_t ctor;
        static size_t dtor;
        static size_t move;
        static size_t stck;
        static size_t gdel;
        static std::string statistical() {
            typedef gc_ptr<void> gc;
            std::stringstream ss;
            {
                ss<<"ctor()#   "<<gc::ctor<<endl;
                ss<<"move()#   "<<gc::move<<endl;
                ss<<"dtor()#   "<<gc::dtor<<endl;
                ss<<"stack()#  "<<gc::stck<<endl;
                ss<<"delete()# "<<gc::gdel<<endl;
                ss<<endl<<"------- INFO -------"<<endl<<endl;
                ss<<"ctor() = stack() + delete()"<<endl;
            }
            return ss.str();
        }
    protected:
        /**
         * check if current ptr is in its move ctor or not
         */
        bool            has_moved = false;
#endif
    protected:
        /**
         * is curren instance located in stack mem.
         */
        bool            is_stack = false;
        /**
         * the containing data
         */
        void*           _data = nullptr;
        /**
         * The possible events enum
         */
        enum class      EVENT { E_CTOR, E_DTOR, E_MOVE, E_DELETE };
        /**
         * The do not delete flag for stop deletion
         * at the end of contained pointer's life
         */
        static void dont_delete(void*)
        { }
        /**
         * The gc deleter handles real ref-count ops for pointers
         */
        template<typename _Tin = T, where
            std::enable_if<
                std::is_convertible<T, _Tin>::value>::type>
        static void gc_delete(T* p)
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
        { }
        /**
         * @brief event operator
         * @param e the event
         * @param p
         */
        static void _event(EVENT e, __unused const void* const p = nullptr) {
            switch(e) {
                case EVENT::E_CTOR:
#ifdef GC_DEBUG
                    gc_ptr<void>::ctor++;
                    cout<<"[CTOR: "<<((self*)p)->get()<<"]";
#endif
                    break;
                case EVENT::E_DTOR:
#ifdef GC_DEBUG
                    if(((self*)p)->has_moved) return;
                    gc_ptr<void>::dtor++;
                    cout<<"[DTOR: "<<((self*)p)->get()<<"]";
#endif
                    break;
                case EVENT::E_MOVE:
#ifdef GC_DEBUG
                    gc_ptr<void>::move++;
                    cout<<"[MOVE: "<<((self*)p)->get()<<"]";
#endif
                    break;
                case EVENT::E_DELETE:
#ifdef GC_DEBUG
                    gc_ptr<void>::gdel++;
                    cout<<"[GDEL: "<<p<<"]"<<endl;
#endif
                    return;
                default:
                    throw std::invalid_argument("Invalid event passed!");
            }
#ifdef GC_DEBUG
            if(((self*)p)->is_stack)
                cout<<"(ON STACK)";
            cout<<endl;
#endif
         }
    public:
        /**
         * get the wrapped pointer with a static cast
         */
        template<typename _Tout = T, where
            std::enable_if<
                can_static_cast(T, _Tout)>::type>
        inline _Tout* get() const
        { return static_cast<_Tout*>(base::get()); }
        /**
         * get the wrapped pointer with a const cast
         */
        template<typename _Tout = T, where
            std::enable_if<
                can_static_cast(T, _Tout)>::type>
        inline const _Tout* get_const() const
        { return const_cast<const _Tout*>(this->get<_Tout>()); }
        /**
         * get the pure wrapped pointer
         */
        inline void* get_pure() const
        { return base::get(); }
        /**
         * the dtor
         */
        inline ~gc_ptr()
        { _event(EVENT::E_DTOR, this); }
        /**
         * for empty ptr
         */
        inline gc_ptr()
            : self(static_cast<T*>(nullptr), self::gc_delete)
        { _event(EVENT::E_CTOR, this); }
        /**
         * for copy assignments [ only for convertable data types ]
         */
        template<typename _Tin, where
            std::enable_if<
                std::is_convertible<_Tin, T>::value>::type>
        inline gc_ptr(const gc_ptr<_Tin>& gp)
        { *this = std::copy(gp); _event(EVENT::E_CTOR, this); }
        /**
         * for move assignments [ only for convertable data types ]
         */
        template<typename _Tin, where
            std::enable_if<
                std::is_convertible<_Tin, T>::value>::type>
        inline gc_ptr(
#ifndef GC_DEBUG
            const
#endif
            gc::gc_ptr<_Tin>&& gp)
        {
#ifdef GC_DEBUG
            gp.has_moved =  true;
#endif
            *this = std::move(gp);
#ifdef GC_DEBUG
            assert(this->has_moved); this->has_moved = false;
#endif
            _event(EVENT::E_MOVE, this); }
        /**
         * for general pointer assignment [only on no<-conversion types between the class' T and the input _Tin]
         */
        template<typename _Tin, where
            std::enable_if<
                std::is_same<T, _Tin>::value>::type>
        inline gc_ptr(_Tin* p)
            : self(p, self::gc_delete)
        { _event(EVENT::E_CTOR, this); }
        /**
         * for stack var assignments
         */
        template<typename _Tin, where
            std::enable_if<
                !std::is_same<_Tin, self>::value &&
                std::is_convertible<_Tin, T>::value &&
                !std::is_pointer<_Tin>::value>::type>   // in restricted mode: this cond. made and ~this cond. make in the
                                                        // below assertion to make sure we stop the stack setting:)
        inline gc_ptr(const _Tin& p)
            : self(const_cast<_Tin*>(std::addressof(p)), self::dont_delete)
        {
            this->is_stack = true;
#ifdef GC_DEBUG
            gc_ptr<void>::stck++;
#endif
            _event(EVENT::E_CTOR, this);
#ifdef GC_RESTRICTED
            static_assert(std::is_pointer<_Tin>::value, "cannot assign stack varibales as managed pointers!");
#endif
        }
        /**
         * for base <- derived assignments [only for derived types, does not accept the same type]
         */
        template<typename D, typename B = T, where
            std::enable_if<
                can_dynamic_cast(B, D)>::type>
        inline gc_ptr(D* p, bool stack_alloced = false)
            : self(dynamic_cast<B*>(p), self::gc_delete)
        { this->is_stack = stack_alloced; _event(EVENT::E_CTOR, this);
        #if GC_DEBUG
            cout<<"(DYNA CAST)"<<endl;
        #endif
        }
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
        /**
         * dynamic cast the containing instance of current ptr to a desired type
         */
        template<typename _Tout> inline gc_ptr<_Tout> as_dynamic_cast() const { return gc_ptr<_Tout>(dynamic_cast<_Tout*>(this->get())); }
        /**
         * static cast the containing instance of current ptr to a desired type
         */
        template<typename _Tout> inline gc_ptr<_Tout> as_static_cast() const { return gc_ptr<_Tout>(static_cast<_Tout*>(this->get())); }
    };
#ifdef GC_DEBUG
    template<> size_t gc_ptr<void>::ctor = 0;
    template<> size_t gc_ptr<void>::dtor = 0;
    template<> size_t gc_ptr<void>::move = 0;
    template<> size_t gc_ptr<void>::stck = 0;
    template<> size_t gc_ptr<void>::gdel = 0;
#endif
    /**
     * a <void*> gc pointer type
     */
    typedef gc_ptr<void> gc_void_ptr_t;

#   undef can_static_cast
#   undef can_dynamic_cast
#   undef can_cast
#   undef deleted_signature
#   undef where
#   undef ptoi
}
#endif // HDR__GC_PTR_HPP
