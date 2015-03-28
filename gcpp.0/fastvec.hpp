/*
* Smieciuch -- portable GC library for C
* (c) 2003, 2004  InForma-Sebastian Kaliszewski
* published under NCSA License
*
* Code is based on a one associated with
* article by William E. Kempf published
* at The Code Project site in Jan 2001
* http://www.codeproject.com/cpp/garbage_collect2.asp
*
* (large) portions by William E. Kempf  2001
*
* fastvec.hpp
* class templates for fast vector of primitive
* objects and it's iterators
*/

#ifndef __cplusplus
#  error "this file works only in C++"
#endif

#if _MSC_VER >= 1000
#  pragma once
#endif

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#endif


#ifndef HDR__FASTVEC_HPP__
#define HDR__FASTVEC_HPP__    1

#ifndef _SMGC_POOR_STL
#  include <cstdlib>
#  include <cstddef>
#else
#  include <stdlib.h>
#  include <stddef.h>
#endif
#include <exception>
#include <stdexcept>
#include "gcafx.hpp"

namespace fv
{
    using namespace std;


    template<typename P>
    struct ptr_rev
    {
        P *ptr;
        ptr_rev(P *p=0): ptr(p) {}

        ptr_rev& operator=(P *p)
        {
            ptr = p;
            return *this;
        }

        P& operator*() { return *ptr; }
        const P& operator*() const { return *ptr; }
        P* operator->() { return ptr; }
        const P* operator->() const { return ptr; }

        ptr_rev& operator--() { ++ptr; return *this; }
        ptr_rev& operator++() { --ptr; return *this; }

        ptr_rev operator--(int) { ptr_rev<P> p=*this; ++ptr; return p; }
        ptr_rev operator++(int) { ptr_rev<P> p=*this; --ptr; return p; }

        ptr_rev& operator-=(ptrdiff_t d) { ptr+=d; return *this; }
        ptr_rev& operator+=(ptrdiff_t d) { ptr-=d; return *this; }
        ptrdiff_t operator-(ptr_rev& p) { return p.ptr-ptr; }

        bool operator<(const ptr_rev& p) { return p.ptr < ptr; }
        bool operator<=(const ptr_rev& p) { return p.ptr <= ptr; }
        bool operator>(const ptr_rev& p) { return p.ptr > ptr; }
        bool operator>=(const ptr_rev& p) { return p.ptr >= ptr; }
        bool operator==(const ptr_rev& p) { return p.ptr == ptr; }
        bool operator!=(const ptr_rev& p) { return p.ptr != ptr; }
        bool operator==(P *p) { return p == ptr; }
        bool operator!=(P *p) { return p != ptr; }
    };

    template<typename P>
    inline ptr_rev<P> operator+(const ptr_rev<P> &p, ptrdiff_t d)
    {
        ptr_rev<P> r = p.ptr-d;
        return r;
    }

    template<typename P>
    inline ptr_rev<P> operator+(ptrdiff_t d, const ptr_rev<P> &p)
    {
        ptr_rev<P> r = d-p.ptr;
        return r;
    }

    template<typename P>
    inline ptr_rev<P> operator-(const ptr_rev<P> &p, ptrdiff_t d)
    {
        ptr_rev<P> r = p.ptr+d;
        return r;
    }

    template<typename P>
    inline ptr_rev<P> operator-(ptrdiff_t d, const ptr_rev<P> &p)
    {
        ptr_rev<P> r = d+p.ptr;
        return r;
    }

    template<typename P>
    inline bool operator==(P *l, const ptr_rev<P> &p)
    {
        return p.ptr == l;
    }

    template<typename P>
    inline bool operator!=(P *l, const ptr_rev<P> &p)
    {
        return p.ptr != l;
    }


    template<typename T>
    class fastvec
    {
        size_t len;
        size_t reserved;
        T* data;

        public:
        typedef T& reference;
        typedef const T& const_reference;
        typedef T* pointer;
        typedef const T* const_pointer;
        typedef T value_type;
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;
        typedef T* iterator;
        typedef const T* const_iterator;
        typedef ptr_rev<T> reverse_iterator;
        typedef ptr_rev<const T> const_reverse_iterator;


        fastvec(): len(0), reserved(64)
        {
            data = (static_cast<T*>(malloc(sizeof(T)*(reserved+1))))+1;
        }

        fastvec(size_t r): len(r), reserved(r+1024)
        {
            data = (static_cast<T*>(malloc(sizeof(T)*(reserved+1))))+1;
        }

        fastvec(size_t r, size_t res): len(r), reserved(res)
        {
            data = (static_cast<T*>(malloc(sizeof(T)*(reserved+1))))+1;
        }

        fastvec(const fastvec &f): len(f.len), reserved(f.reserved)
        {
            data = (static_cast<T*>(malloc(sizeof(T)*(reserved+1))))+1;
            memcpy(data, f.data, sizeof(T)*len);
        }

        ~fastvec() { free(data-1); }


        fastvec& operator =(const fastvec &f)
        {
            reserved = f.reserved;
            len = f.len;
            data = (static_cast<T*>(realloc(data-1, sizeof(T)*(reserved+1))))+1;
            memcpy(data, f.data, sizeof(T)*(reserved-1));
            return *this;
        }

        void reserve(size_t r)
        {
            reserved = r;
            data = (static_cast<T*>(realloc(data-1, sizeof(T)*(reserved+1))))+1;
        }

        void erase(iterator w)
        {
            iterator e = data+len;//-1;
            if(w == e/*+1*/)
            {
                return;
            }
            //else if(w == e)
            //{
                //  len--;
                //  return;
            //}
            for(;w!=e; ++w)
            {
                *w = *(w+1);
            }
            if(len*4+4096 < reserved)
            reserve(len*2+2048);
            len--;
        }

        void erase(iterator f, iterator l)
        {
            if(f == l)
            return;
            ptrdiff_t d = l-f;
            iterator e = data+len;
            for(;l!=e; ++l)
            {
                *f++ = *l;
            }
            len -= d;
            if(len*4+4096 < reserved)
            reserve(len*2+2048);
        }

        void push_back(const_reference val)
        {
            if(len >= reserved)
            reserve(len*2+2048);
            data[len++] = val;
        }

        void pop_back()
        {
            if(len*4+4096 < reserved)
            reserve(len*2+2048);
            --len;
        }

        void resize(size_t r)
        {
            len = r;
            if(len >= reserved)
            reserve(len*2+2048);
        }

        void clear() { len = 0; }

        operator pointer() { return data; }
        operator const_pointer() const { return data; }

        size_t size() const { return len; }
        size_t capacity() const { return reserved; }
        bool empty() const { return len == 0; }

        reference front() { return *data; }
        const_reference front() const { return *data; }
        reference back() { return data[len-1]; }
        const_reference back() const { return data[len-1]; }

        iterator begin() { return data; }
        const_iterator begin() const { return data; }
        reverse_iterator rbegin() { return data+len-1; }
        const_reverse_iterator rbegin() const { return data+len-1; }

        iterator end() { return data+len; }
        const_iterator end() const { return data+len; }
        reverse_iterator rend() { return data-1; }
        const_reverse_iterator rend() const { return data-1; }
    };

}

#endif
