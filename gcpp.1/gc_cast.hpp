#ifndef GC_CAST_HPP
#define GC_CAST_HPP
#if __cplusplus <= 199711L
#   error "This cannot be use for under C++11 version."
#endif
#include <typeinfo>        // std::bad_cast
#include <type_traits>
#include "gcafx.hpp"
namespace gc {
    /**
     * automatically performs dynamic or static cast to input
     * throws bad_cast if none of above casts are suitable for input to output cast
     */
    template <typename T> struct remove_all_ref_ptr{ typedef T type; };
    template <typename T> struct remove_all_ref_ptr<T *>                : public remove_all_ref_ptr<T>{ };
    template <typename T> struct remove_all_ref_ptr<T * const>          : public remove_all_ref_ptr<T>{ };
    template <typename T> struct remove_all_ref_ptr<T * volatile>       : public remove_all_ref_ptr<T>{ };
    template <typename T> struct remove_all_ref_ptr<T * const volatile> : public remove_all_ref_ptr<T>{ };
    template <typename T> struct remove_all_ref_ptr<T &>                : public remove_all_ref_ptr<T>{ };
    template <typename T> struct remove_all_ref_ptr<T &&>               : public remove_all_ref_ptr<T>{ };
    template <typename T> struct base_type : public ::std::remove_cv<typename remove_all_ref_ptr<T>::type>{ };

#define where typename = typename

#define dyna_cast(_Tout, _Tin) \
            (!std::is_void<typename base_type<_Tin>::type>::value && !std::is_void<typename base_type<_Tout>::type>::value && \
            std::is_compound<typename base_type<_Tin>::type>::value && \
            std::is_base_of<typename base_type<_Tin>::type, typename base_type<_Tin>::type>::value)

#define stat_cast(_Tout, _Tin) \
            (std::is_convertible<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value && \
            std::is_fundamental<typename base_type<_Tin>::type>::value && \
            std::is_fundamental<typename base_type<_Tout>::type>::value)

    /**
     * pointers cast region
     */
    template<typename _Tout, typename _Tin>
    inline _Tout gc_cast(_Tin p, typename
        std::enable_if<
            !std::is_same<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value &&
            (std::is_pointer<_Tout>::value && std::is_pointer<_Tin>::value) &&
            dyna_cast(_Tout, _Tin)>::type* = 0)
    { return dynamic_cast<_Tout>(p); }

    template<typename _Tout, typename _Tin>
    inline _Tout gc_cast(_Tin p, typename
        std::enable_if<
            !std::is_same<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value &&
            (std::is_pointer<_Tout>::value && std::is_pointer<_Tin>::value) &&
            stat_cast(_Tout, _Tin)>::type* = 0)
    { return static_cast<_Tout>(p); }

    template<typename _Tout, typename _Tin>
    inline _Tout gc_cast(_Tin, typename
        std::enable_if<
            !std::is_same<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value &&
            (std::is_pointer<_Tout>::value && std::is_pointer<_Tin>::value) &&
            !dyna_cast(_Tout, _Tin) && !stat_cast(_Tout, _Tin)>::type* = 0)
    { throw std::bad_cast(); }
    /**
     * non-pointers cast region
     */
    template<typename _Tout, typename _Tin>
    inline _Tout gc_cast(_Tin p, typename
        std::enable_if<
            std::is_same<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value>::type* = 0)
    { return p; }

    template<typename _Tout, typename _Tin>
    inline _Tout gc_cast(_Tin p, typename
         std::enable_if<
             !std::is_same<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value &&
             !(std::is_pointer<_Tout>::value || std::is_pointer<_Tin>::value) &&
             dyna_cast(_Tout, _Tin)>::type* = 0)
    { return dynamic_cast<_Tout>(p); }

    template<typename _Tout, typename _Tin>
    inline _Tout gc_cast(_Tin p, typename
         std::enable_if<
             !std::is_same<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value &&
             !(std::is_pointer<_Tout>::value || std::is_pointer<_Tin>::value) &&
             stat_cast(_Tout, _Tin)>::type* = 0)
    { return static_cast<_Tout>(p); }

    template<typename _Tout, typename _Tin>
    inline _Tout& gc_cast(_Tin&, typename
         std::enable_if<
             !std::is_same<typename base_type<_Tin>::type, typename base_type<_Tout>::type>::value &&
             !(std::is_pointer<_Tout>::value || std::is_pointer<_Tin>::value) &&
             !dyna_cast(_Tout, _Tin) && !stat_cast(_Tout, _Tin)>::type* = 0)
    { throw std::bad_cast(); }

    template<typename _Tout>
    inline _Tout* gc_cast(nullptr_t)
    { return static_cast<_Tout*>(nullptr); }

#undef dyna_cast
#undef stat_cast
#undef where
}
#endif // GC_CAST_HPP
