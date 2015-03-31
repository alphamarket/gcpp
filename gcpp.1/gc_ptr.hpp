#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <cstdlib>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#ifdef GCPP_DEBUG
#   include <sstream>
#endif

#include "gcafx.hpp"
namespace gc {
#   define where                        typename = typename
#   define can_cast(FROM, TO)           std::is_convertible<FROM, TO>::value
#   define can_cast_ptr(FROM, TO)       std::is_convertible<FROM*, TO*>::value
#   define can_dynamic_cast(FROM, TO)   can_cast(FROM, TO) && !std::is_same<FROM, TO>::value && std::is_class<TO>::value && !std::is_const<FROM>::value && std::is_base_of<TO, FROM>::value
#   define can_static_cast(FROM, TO)    can_cast(FROM, TO)
    using namespace std;
    template<typename T> class gc_ptr;
    class gc_map {
        template<typename T> friend class       gc_ptr;
        typedef unordered_map<void*, size_t>    map_t;
        /**
         * The address map container
         */
        map_t           _gc_map;
        /**
         * The static instance of current class(if it get lost, we lost all the _gc_map)
         */
        static gc_map   _instance;
    protected:
        /**
         * ref# up a pointer and return the new ref#
         */
        inline static size_t ref_up(void* p) {
            if(p == nullptr) return 0;
            size_t c = 0;
            if(gc_map::instance()._gc_map.count(p))
                c = ++gc_map::instance()._gc_map[p];
            else
                gc_map::instance()._gc_map.insert({p, (c = 1)});
#ifdef GCPP_DEBUG
            cout<<"\033[32m[ref][^] -> "<<c<<"\033[m";
#endif
            return c;
        }
        /**
         * ref# up a pointer and return the new ref#
         */
        inline static size_t ref_down(void* p) {
            if(p == nullptr) return -1;
            size_t c = 0;
            assert(gc_map::instance()._gc_map.count(p) && (c = gc_map::instance()._gc_map[p]--) && c--);
            if(c == 0)
                gc_map::instance()._gc_map.erase(p);
#ifdef GCPP_DEBUG
            cout<<"\033[95m[ref][v] -> "<<c<<"\033[m";
#endif
            return c;
        }
        inline static gc_map& instance()    { return gc_map::_instance; }
        /**
         * suppress instantization of this class for public
         */
        inline explicit gc_map() { }
    public:
        /**
         * get the gc' map instance
         */
        inline static const map_t&  get()   { return gc_map::_instance._gc_map; }

    };
    /**
     * init the static member of gc_map::_instance
     */
    gc_map gc_map::_instance;
    struct _cast {};
    struct _dynamic_cast : _cast {};
    struct _static_cast  : _cast {};

    /**
     * class gc_ptr decl. this class is to manage pointers to objects
     * so it is not logical to accept pointers as input
     */
    template<typename T>
    class gc_ptr                                                    : protected std::shared_ptr<void> {
        static_assert(!std::is_pointer<T>::value, "cannot accept pointers as type!");
        typedef std::shared_ptr<void>           base;
        typedef gc_ptr                          self;
        typedef gc_ptr<void>                    _static;
        typedef void (*deleter_signature)(T*);
    public:
#ifdef GCPP_DISABLE_HEAP_ALLOC
        /**
         * disallow heap allocation for gc_ptr
         * its is here to manage memory, if we give the client to allocate gc_ptr
         * on heap, it may cause mem. losses on gc_ptr own allocated memory
         * if the user was not careful, not the wrapped ptr.
         */
        void * operator new       (size_t) = delete;
        void * operator new[]     (size_t) = delete;
        void   operator delete    (void *) = delete;
        void   operator delete[]  (void *) = delete;
#endif
    protected:
#ifdef  GCPP_DEBUG
    public:
        static size_t ctor;
        static size_t cnew;
        static size_t dtor;
        static size_t move;
        static size_t stck;
        static size_t gdel;
        static std::string statistical() {
            std::stringstream ss;
            {
                ss<<"cnew()#   "<<_static::cnew<<endl;
                ss<<"ctor()#   "<<_static::ctor<<endl;
                ss<<"move()#   "<<_static::move<<endl;
                ss<<"dtor()#   "<<_static::dtor<<endl;
                ss<<"stack()#  "<<_static::stck<<endl;
                ss<<"delete()# "<<_static::gdel<<endl;
                ss<<endl<<"------- INFO -------"<<endl<<endl;
                ss<<"cnew() == delete()"<<endl;
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
        static void dont_delete(T*)
        {
#ifdef GCPP_DEBUG
            cout<<"\033[33m(SKIPPED DELETE)\033[m"<<endl;
#endif
        }
        /**
         * The gc deleter handles real ref-count ops for pointers
         */
        static void gc_delete(T* p)
        {
            size_t c = gc_map::ref_down(p);
            if(c == 0) {
#ifdef GCPP_DEBUG
                gc_ptr<void>::gdel++;
                cout<<"\033[95m[GDEL: "<<p<<"]\033[m";
                cout<<"\033[31m(DELETE) \033[m"<<endl;
#endif
                _event(EVENT::E_DELETE, p);
                if(std::is_void<T>::value) free(p);
                else delete(p);
                p = NULL;
                return;
            }
            _event(EVENT::E_DELETE, p);
        }
        /**
         * for general pointer delete constructor [ The return sorce of other ctor ]
         */
        template<typename _Tin, typename _Delete, where
            std::enable_if<
                std::is_pointer<_Delete>::value &&
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(_Tin* p, _Delete d)
            : base(p, d)
        { }
        /**
         * event operator
         */
        static void _event(EVENT e, __unused const void* const p = nullptr) {
            switch(e) {
                case EVENT::E_CTOR:
#ifdef GCPP_DEBUG
                    gc_ptr<void>::ctor++;
                    cout<<"\033[32m[CTOR: "<<((self*)p)->get_pure()<<"] \033[m";
#endif
                    if(!((self*)p)->is_stack)
                        gc_map::ref_up(((self*)p)->get_pure());
                    break;
                case EVENT::E_DTOR:
#ifdef GCPP_DEBUG
                    if(((self*)p)->has_moved) return;
                    gc_ptr<void>::dtor++;
                    cout<<"[DTOR: "<<((self*)p)->get()<<"]";
#endif
                    break;
                case EVENT::E_MOVE:
#ifdef GCPP_DEBUG
                    gc_ptr<void>::move++;
                    cout<<"[MOVE: "<<((self*)p)->get()<<"]";
#endif
                    break;
                case EVENT::E_DELETE:
#ifdef GCPP_DEBUG
#endif
                    return;
                default:
                    throw std::invalid_argument("Invalid event passed!");
            }
#ifdef GCPP_DEBUG
            if(((self*)p)->is_stack)
                cout<<"\033[33m(ON STACK)\033[m";
            else if(e == EVENT::E_CTOR && gc_map::get().count(((self*)p)->get_pure()) && gc_map::get().at(((self*)p)->get_pure()) == 1 && ++gc_ptr<void>::cnew)
                cout<<"\033[32m(CREATE) \033[m";
            cout<<endl;
#endif
#undef      wrapped_ptr
        }
        /**
         * get proper deleter based on input arg's allocation status
         */
        template<typename _Tin>
        inline deleter_signature gc_get_deleter(const gc_ptr<_Tin>& gp) {
            if(gp.stack_referred())
                return self::dont_delete;
            return self::gc_delete;
        }
    public:
        /**
         * check if the pointer refers to location on stack memory
         */
        inline bool stack_referred() const { return this->is_stack; }
        /**
         * for empty ptr
         */
        inline gc_ptr()
            : self(static_cast<T*>(nullptr), self::dont_delete)
        { _event(EVENT::E_CTOR, this); }
        /**
         * the dtor
         */
        inline ~gc_ptr()
        { _event(EVENT::E_DTOR, this); }
        /**
         * for copy assignments [ only for convertable data types ]
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(const gc_ptr<_Tin>& gp)
        {
            if(can_dynamic_cast(_Tin, T))
                this->reset(dynamic_cast<T*>(gp.get()), this->gc_get_deleter(gp));
            else if(can_static_cast(_Tin, T))
                this->reset(static_cast<T*>(gp.get()), this->gc_get_deleter(gp));
            else throw bad_cast();
            this->is_stack = gp.stack_referred();
            _event(EVENT::E_CTOR, this);
        }
        /**
         * for move assignments [ only for convertable data types ]
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(
#ifndef GCPP_DEBUG
            const
#endif
            gc::gc_ptr<_Tin>&& gp)
        {
#ifdef GCPP_DEBUG
            gp.has_moved =  true;
#endif
            *this = std::move(gp);
#ifdef GCPP_DEBUG
            assert(this->has_moved); this->has_moved = false;
#endif
            _event(EVENT::E_MOVE, this); }
        /**
         * for general pointer assignment [only on no<-conversion types between the class' T and the input _Tin]
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(_Tin* p)
            : self(static_cast<T*>(p), self::gc_delete)
        { _event(EVENT::E_CTOR, this); }
        /**
         * for stack var assignments
         */
        template<typename _Tin, where
            std::enable_if<
                !std::is_same<_Tin, self>::value &&
                can_cast_ptr(_Tin, T) &&
                !std::is_pointer<_Tin>::value>::type>   // in restricted mode: this cond. made and ~this cond. make in the
                                                        // below assertion to make sure we stop the stack setting:)
        inline gc_ptr(const _Tin& p)
            : self(const_cast<_Tin*>(std::addressof((T&)p)), self::dont_delete)
        {
            // this is an stack memory
            this->is_stack = true;
            // considering above std::enable_if conditions there is a need for casting
            // we won't touch the wrapped pointer :)
            this->operator*() = static_cast<T>(p);
#ifdef GCPP_DEBUG
            gc_ptr<void>::stck++;
#endif
            _event(EVENT::E_CTOR, this);
#ifdef GCPP_RESTRICTED
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
#if GCPP_DEBUG
            cout<<"(DYNA CAST)"<<endl;
#endif
        }
        /**
         * disposes the wrapped ptr
         * @note based on how many gc_ptr referes to wrapped ptr it may or may not free(delete) the
         * wrapped ptr.
         */
        void dispose() { this->reset(); }
        /**
         * assignment oprator
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr<T>& operator =(const gc_ptr<_Tin>& gp) {
            *this = self(gp);
            return *this;
        }
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
         * dynamic cast the containing instance of current ptr to a desired type
         */
        template<typename _Tout> inline gc_ptr<_Tout> as_dynamic_cast() const { return gc_ptr<_Tout>(dynamic_cast<_Tout*>(this->get())); }
        /**
         * static cast the containing instance of current ptr to a desired type
         */
        template<typename _Tout> inline gc_ptr<_Tout> as_static_cast() const { return gc_ptr<_Tout>(static_cast<_Tout*>(this->get())); }
        /**
         * get use_count of current instance
         */
        size_t use_count() const {
            // if not registered in map
            if(!gc_map::get().count(this->get_pure())) {
                // this should be a `dont_delete` pointer type
                assert(this->stack_referred());
                // just consider the base's count as it is
                return base::use_count();
            } else
                // return the actual reference#
                return gc_map::get().at(this->get_pure());
        }
        /**
         * get the pure wrapped pointer
         */
        inline void* get_pure() const
        { return base::get(); }
        /**
         * for access the wrapped pointer's members
         */
        inline T* operator->() { return this->get(); }
        /**
         * for access the wrapped pointer's members [valid for all except <void*> types]
         */
        template<typename _Tout = T, where
            std::enable_if<
                !std::is_void<_Tout>::value>::type>
        inline const _Tout& operator* () const { return *(this->get<_Tout>()); }
        template<typename _Tout = T, where
            std::enable_if<
                !std::is_void<_Tout>::value>::type>
        inline _Tout& operator* () { return *(this->get<_Tout>()); }
    };
#ifdef GCPP_DEBUG
    template<> size_t gc_ptr<void>::ctor = 0;
    template<> size_t gc_ptr<void>::cnew = 0;
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
}
#endif // HDR__GC_PTR_HPP
