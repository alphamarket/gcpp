#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <cstdlib>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include "gcafx.hpp"
#include "gc_cast.hpp"
namespace gc {
#   define where                                        typename = typename
#   define can_cast(FROM, TO)                           std::is_convertible<FROM, TO>::value
#   define can_cast_ptr(FROM, TO)                       std::is_convertible<FROM*, TO*>::value
    using namespace std;
    template<typename T>                                class gc_ptr;
    typedef intptr_t                                    gc_intptr_t;
    class gc_map {
        template<typename T> friend class               gc_ptr;
        typedef unordered_map<gc_intptr_t, size_t>      map_t;
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
        inline static size_t ref_up(const gc_intptr_t& p) {
            if(p == 0) return 0;
            size_t c = 0;
            if(gc_map::instance()._gc_map.count(p))
                c = ++gc_map::instance()._gc_map[p];
            else
                gc_map::instance()._gc_map.insert({p, (c = 1)});
            return c;
        }
        /**
         * ref# up a pointer and return the new ref#
         */
        inline static size_t ref_down(const gc_intptr_t& p) {
            if(p == 0 || !gc_map::instance()._gc_map.count(p)) return 0;
            size_t c = --gc_map::instance()._gc_map[p];
            if(c == 0)
                gc_map::instance()._gc_map.erase(p);
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
    class gc_ptr                                                    : protected std::shared_ptr<gc_ptr<T>> {
        static_assert(!std::is_pointer<T>::value, "cannot accept pointers as type!");
        typedef std::shared_ptr<gc_ptr<T>>      base;
        typedef gc_ptr                          self;
        typedef gc_ptr<void>                    _static;
        typedef void (*deleter)(const self*);
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
        static const
        gc_intptr_t     NOT_REGISTERED = 0;
        /**
         * is curren instance located in stack mem.
         */
        bool            _is_stack = false;
        /**
         * the containing data
         */
        T*              _data = nullptr;
        /**
         * the id which current value has registered
         */
        gc::gc_intptr_t _register_id  = self::NOT_REGISTERED;
        /**
         * check if current instance has disposed
         */
        bool            _disposed = false;
        /**
         * The possible events enum
         */
        enum class      EVENT { E_CTOR, E_DTOR, E_MOVE, E_DELETE };
        /**
         * The do not delete flag for stop deletion
         * at the end of contained pointer's life
         */
        static void dont_delete(const self*) { /* do nothing */ }
        /**
         * The gc deleter handles real ref-count ops for pointers
         */
        static void gc_delete(const self* p)
        {
            if(p->has_disposed()) return;
            invoke_event(EVENT::E_DELETE);
            if(!gc_map::ref_down(p->get_id()))
                delete p->get();
        }
        /**
         * event operator
         */
        static void invoke_event(EVENT e, const self* const p = nullptr) {
            switch(e) {
                case EVENT::E_CTOR:
                    if(!p->_is_stack)
                        gc_map::ref_up(p->get_id());
                    break;
                case EVENT::E_DTOR:     break;
                case EVENT::E_MOVE:     break;
                case EVENT::E_DELETE:   break;
                default: throw std::invalid_argument("Invalid event passed!");
            }
        }
        /**
         * get proper deleter based on input arg's allocation status
         */
        template<typename _Tin>
        inline deleter gc_get_deleter(const gc_ptr<_Tin>& gp) {
            if(gp.stack_referred())
                return self::dont_delete;
            return self::gc_delete;
        }
        /**
         * get the registration id of input pointer
         */
        template<typename _Tin>
        inline gc_intptr_t get_id(_Tin* p) const { return reinterpret_cast<gc_intptr_t>(p); }
        /**
         * for general pointer delete constructor [ The return sorce of other ctor ]
         */
        template<typename _Delete, where
            std::enable_if<
                std::is_pointer<_Delete>::value>::type>
        inline gc_ptr(T* data, gc_intptr_t reg_id, _Delete d, bool is_stack = false)
            : base(this, d), _is_stack(is_stack), _data(data), _register_id(reg_id)
        { }
    public:
        /**
         * for empty ptr
         */
        inline gc_ptr()
            : self(nullptr, self::NOT_REGISTERED, self::dont_delete, true)
        /* everything is already init to their defualts */
        { self::invoke_event(EVENT::E_CTOR, this); }
        /**
         * the dtor
         */
        inline ~gc_ptr()
        { /* the deleter functions will take care of it automatically */ }
        /**
         * for normal convertable heap assignments
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(_Tin* in)
            : self(gc_cast<T*>(in), this->get_id(in), self::gc_delete, false)
        { self::invoke_event(EVENT::E_CTOR, this); }
        /**
         * for reference stack vars. assignments
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast(_Tin, T) &&
                !std::is_pointer<_Tin>::value>::type>
        inline gc_ptr(_Tin& in)
            : self(
                std::addressof(gc_cast<T&>(in)),
                this->get_id(std::addressof(in)),
                self::dont_delete,
                true)
        { self::invoke_event(EVENT::E_CTOR, this); }
        /**
         * copy ctor
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(const gc_ptr<_Tin>& gp)
            : self(
                gc_cast<T*>(gp.get()),
                gp.get_id(),
                this->gc_get_deleter(gp),
                gp.stack_referred())
        { self::invoke_event(EVENT::E_CTOR, this);  }
        /**
         * move ctor
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(const gc_ptr<_Tin>&& gp)
        { *this = std::move(gp); }
        /**
         * disposes the wrapped ptr
         * @note based on how many gc_ptr referes to wrapped ptr it may or may not free(delete) the
         * wrapped ptr.
         */
        void dispose() {
            if(this->_disposed) return;
            this->reset();
            this->_disposed = true;
        }
        /**
         * check if current instance has been disposed
         */
        bool has_disposed() const { return this->_disposed; }
        /**
         * assignment oprator
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr<T>& operator =(const gc_ptr<_Tin>& gp) { *this = self(gp); return *this; }
        /**
         * get the wrapped pointer with a static cast
         */
        inline T* get() const
        { return this->_data; }
        /**
         * get the wrapped pointer with a const cast
         */
        inline const T* get_const() const
        { return const_cast<const T*>(this->get()); }
        /**
         * get use_count of current instance
         */
        size_t use_count() const {
            // if not registered in map
            if(!gc_map::get().count(this->get_id())) {
                // this should be a `dont_delete` pointer type
                assert(this->stack_referred());
                // just consider the base's count as it is
                return base::use_count();
            } else
                // return the actual reference#
                return gc_map::get().at(this->get_id());
        }
        /**
         * get the registration id for the wrapped ptr
         */
        inline gc_intptr_t get_id() const
        { return this->_register_id; }
        /**
         * check if the pointer refers to location on stack memory
         */
        inline bool stack_referred() const { return this->_is_stack; }
        /**
         * for access the wrapped pointer's members
         */
        inline T* operator->() { return this->get(); }
        /**
         * for access the wrapped pointer's members
         */
        inline T& operator* () const { return *(this->get()); }
        inline T& operator* () { return *(this->get()); }
    };
#undef can_cast_ptr
#undef can_cast
#undef where
}
#endif // HDR__GC_PTR_HPP
