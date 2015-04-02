#ifndef HDR__GC_PTR_HPP
#define HDR__GC_PTR_HPP
#include <memory>
#include <cstdlib>
#include <malloc.h>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <stdexcept>

#include "gcafx.hpp"
#include "gc_map.hpp"
#include "gc_cast.hpp"
#include "gc_detail.hpp"
namespace gc {
#   define where \
        typename = typename
#   define can_cast(FROM, TO) \
        std::is_convertible<FROM, TO>::value
#   define can_cast_ptr(FROM, TO) \
        std::is_convertible<FROM*, TO*>::value
#   define gc_smart_virtual_destructor_check(T, _Tin) \
        static_assert(!std::is_compound<typename base_type<T>::type>::value || \
            std::is_same<typename base_type<T>::type, typename base_type<_Tin>::type>::value || \
            (std::is_base_of<typename base_type<T>::type, typename base_type<_Tin>::type>::value && \
            std::has_virtual_destructor<typename base_type<T>::type>::value) \
        ,"expecting the types to have accessible virtual destructor, but there is not one!")
    template<typename T>
    class gc_ptr: protected std::shared_ptr<gc_detail<T>> {
        /**
         * class gc_ptr decl. this class is to manage pointers to objects
         * so it is not logical to accept pointers as input
         */
        static_assert(!std::is_pointer<T>::value, "cannot accept pointers as type!");
        typedef std::shared_ptr<gc_ptr<T>>      base;
        typedef gc_ptr                          self;
        typedef void (*deleter)(gc_detail<T>*);
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
        static void dont_delete(gc_detail<T>* d) { delete d; }
        /**
         * The gc deleter handles real ref-count ops for pointers
         */
        static void gc_delete(gc_detail<T>* d)
        {
            invoke_event(EVENT::E_DELETE);
            // if the use case came down to zero?
            if(!gc_map::ref_down(d->get_id()))
            // free the memory
            { gc_delete_helper(d); }
        }
        /**
         * helpers to use in `gc_delete()`
         */
        template<typename _Tin, where
            std::enable_if<
                !std::is_void<_Tin>::value>::type>
        static void gc_delete_helper (gc_detail<_Tin>*& d)
        // free non-void data types
        { delete d->get_data(), delete d; }
        static void gc_delete_helper (gc_detail<void>*& d)
        // free     void data types
        { free(d->get_data()), delete d; }
        /**
         * event operator
         */
        static void invoke_event(EVENT e, const self* const p = nullptr) {
            switch(e) {
                case EVENT::E_CTOR:
                    if(!p->stack_referred())
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
        inline deleter gc_get_deleter(const gc_ptr<_Tin>& gp)
        { if(gp.stack_referred()) return self::dont_delete; return self::gc_delete; }
        /**
         * get the registration id of input pointer
         */
        template<typename _Tin>
        inline gc_id_t get_id(_Tin* p) const
        { return reinterpret_cast<gc_id_t>(p); }
        /**
         * for general pointer delete constructor [ The return sorce of other ctor ]
         */
        template<typename _Delete, where
            std::enable_if<
                std::is_pointer<_Delete>::value>::type>
        inline gc_ptr(T* data, gc_id_t reg_id, _Delete d, bool is_stack = false)
        { this->reset(new gc_detail<T>(data, reg_id, is_stack, false), d); }
    public:
        /**
         * for empty ptr
         */
        inline gc_ptr()
            : self(nullptr, gc_detail<T>::NOT_REGISTERED, self::dont_delete, true)
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

        { gc_smart_virtual_destructor_check(T, _Tin); self::invoke_event(EVENT::E_CTOR, this); }
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
        { gc_smart_virtual_destructor_check(T, _Tin); self::invoke_event(EVENT::E_CTOR, this); }
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
        { gc_smart_virtual_destructor_check(T, _Tin); self::invoke_event(EVENT::E_CTOR, this); }
        /**
         * move ctor
         */
        template<typename _Tin, where
            std::enable_if<
                can_cast_ptr(_Tin, T)>::type>
        inline gc_ptr(const gc_ptr<_Tin>&& gp)
        {
            gc_smart_virtual_destructor_check(T, _Tin);
            *this = std::move(gp);
        }
        /**
         * disposes the wrapped ptr
         * @note based on how many gc_ptr referes to wrapped ptr it may or may not free(delete) the
         * wrapped ptr.
         */
        void dispose() { if(this->_disposed) return; this->reset(); this->_disposed = true; }
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
        { return std::shared_ptr<gc_detail<T>>::get()->get_data(); }
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
                return std::shared_ptr<gc_detail<T>>::use_count();
            } else
                // return the actual reference#
                return gc_map::get().at(this->get_id());
        }
        /**
         * check if the pointer refers to location on stack memory
         */
        inline bool stack_referred() const { return std::shared_ptr<gc_detail<T>>::get()->stack_referred(); }
        /**
         * get the registration id for the wrapped ptr
         */
        inline gc_id_t get_id() const
        { return std::shared_ptr<gc_detail<T>>::get()->get_id(); }
        /**
         * for access the wrapped pointer's members
         */
        inline T* operator->() { return this->get(); }
        /**
         * for access the wrapped pointer's members
         */

        inline typename std::add_lvalue_reference<T>::type
        operator* () const { return *(this->get()); }
        inline typename std::add_lvalue_reference<T>::type
        operator* () { return *(this->get()); }
    };
#undef gc_smart_virtual_destructor_check
#undef can_cast_ptr
#undef can_cast
#undef where
}
#endif // HDR__GC_PTR_HPP
