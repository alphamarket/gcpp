#ifndef HDR__GC_ARRAY_PTR_HPP
#define HDR__GC_ARRAY_PTR_HPP
#include <vector>
#include "gc_ptr.hpp"
namespace gc {
    template<typename T>
    class gc_array_ptr : protected gc_ptr<gc_array_ptr<T>>, public std::vector<gc::gc_ptr<T>> {
        typedef gc::gc_array_ptr<T>         self;
        typedef gc::gc_ptr<T>               array_elem;
    public:
        /**
         * allocate the sized array, with an allocator delegate
         */
        gc_array_ptr(size_t size, T*(*allocator)(size_t))
        { while(size--) this->push_back(allocator(size)); }
        /**
         * allocate the sized array, for constructable types
         */
        gc_array_ptr(size_t size, typename std::enable_if<std::is_constructible<T>::value>::type* =0)
            : self(size, [](size_t){ return new T; })
        { }
        /**
         * fill the array from the list
         */
        gc_array_ptr(std::initializer_list<array_elem> arr)
            : std::vector<array_elem>(arr)
        { }
        /**
         * get pointer-value of an index ptr
         */
        T* operator[](size_t index) const
        {
            if(index >= this->size())
                throw std::out_of_range("index out of range");
            return std::vector<gc::gc_ptr<T>>::operator[](index).get();
        }
    };
}
#endif // GC_PTR_ARRAY_H
