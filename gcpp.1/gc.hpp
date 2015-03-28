#ifndef HDR__GC_HPP__
#define HDR__GC_HPP__

#if __cplusplus <= 199711L
#   error "This cannot be use for under C++11 version."
#endif

#include <memory>
#include <vector>
#include <algorithm>
#include "gcafx.hpp"
#include "flags.hpp"
#include "gc_ptr.hpp"
#include "typedefs.hpp"

void* operator new(size_t s, const ::GC&)
{
  return ::operator new(s);
}

namespace gc {
    using namespace std;
    template<typename T>
    class gc_node {
        void* _address;
        gc_ptr<T>* _val;
    public:
        gc_node(gc_ptr<T>* const gp) {
            this->_address = static_cast<void*>(gp->get());
            this->_val = gp;
        }
        weak_ptr<T> getVal()        const { this->_val; }
        const void* getAddress()    const { return this->_address; }
    };
    class gc_heap {

    };
}

#endif
