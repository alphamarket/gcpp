/*
* Smieciuch -- portable GC library for C
* (c) 2003-2005  InForma-Sebastian Kaliszewski
* published under NCSA License
*
* Code is based on a one associated with
* article by William E. Kempf published
* at The Code Project site in Jan 2001
* http://www.codeproject.com/cpp/garbage_collect2.asp
*
* (large) portions by William E. Kempf  2001
*
* gcpp.cpp
* classes for C++ port of the colactor
*/

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#endif

#ifndef _SMGC_POOR_STL
#  include <cstdlib>
#  include <memory>
#  include <climits>
#  include <cstring>
#  include <limits>
#else
#  include <stdlib.h>
#  include <limits.h>
#  include <memory.h>
#  include <string.h>
#endif
#include <set>
#include <map>
//#include <vector>
#include <deque>
#include <utility>
#include <algorithm>
#include <exception>
#include <stdexcept>

#ifndef NDEBUG
# include <iostream>
#endif

#include "gc.hpp"
#include "gcafx.hpp"
#include "fastvec.hpp"

using namespace std;
using namespace fv;

namespace
{
    struct node_t: public gc::detail::node_base
    {
        node_t(const void* obj, size_t n, bool intern);
        ~node_t() {}
        bool contains(const void* obj);

        const void* base;
        size_t size;
        bool mark;
        bool internal;
        void* object;
        void (*destroy)(void*, void*, size_t);

        static void kill(node_t *node);
        static void* operator new(size_t s);
        static void* operator new(size_t s, void *place);
        static void operator delete(void *n);
        static void operator delete(__unused void *n, __unused void *place) {}
    };

    struct node_less
    {
        bool operator()(const node_t* x, const node_t* y) const
        {
            //return (static_cast<const char*>(x->base) + x->size) <= static_cast<const char*>(y->base);
            static const less<const void*> cmp = less<const void*>();
            return !cmp(static_cast<const char*>(y->base), static_cast<const char*>(x->base) + x->size);
        }
    } node_cmp;

    typedef pair<gc::detail::pointer_base*, node_t*> ptr_pair;
    typedef pair<gc::detail::weak_base*, node_t*> weak_pair;
    typedef fastvec<ptr_pair> ptr_vector;
    typedef fastvec<node_t*> node_vector;
    typedef fastvec<weak_pair> weak_vector;
    typedef deque<ptr_vector::iterator> ptr_queue;

    struct data_t
    {
        data_t() : threshold(gc::default_threshold), dynamic_threshold(gc::default_threshold),
        allocated(0), collecting(false), current_mark(false)
        { /*InitializeCriticalSection(&cs);*//*cs=false;*/ }

        ~data_t()
        {
            // If there is something to collect, then we do collections until
            // nothing could be freed anymore
            size_t pre = old_nodes.size() + new_nodes.size() + born_nodes.size();
            if(pre)
            {
                size_t post = pre;
                do
                {
                    pre = post;
                    gc::collect();
                    post = old_nodes.size() + new_nodes.size() + born_nodes.size();
                } while(post < pre && post != 0);
                pre = post;
            }

            // After final collection we clean up remaining undeleted nodes,
            // but do not call destructors of objects belonging to them
            if(pre)
            {
                node_vector::iterator end = old_nodes.end();
                for(node_vector::iterator i=old_nodes.begin(); i<end; ++i)
                {
                    (*i)->destroy = 0;
                    node_t::kill(*i);
                }
                end = new_nodes.end();
                for(node_vector::iterator j=new_nodes.begin(); j<end; ++j)
                {
                    (*j)->destroy = 0;
                    node_t::kill(*j);
                }
                end = born_nodes.end();
                for(node_vector::iterator k=born_nodes.begin(); k<end; ++k)
                {
                    (*k)->destroy = 0;
                    node_t::kill(*k);
                }
            }
            /*DeleteCriticalSection(&cs);*/
        }

        ptr_vector old_pointers;
        ptr_vector new_pointers;
        weak_vector weak_pointers;
        //node_set nodes;
        node_vector old_nodes;
        node_vector new_nodes;
        node_vector born_nodes;
        size_t threshold;
        size_t dynamic_threshold;
        size_t allocated;
        bool collecting;
        bool current_mark;
        //CRITICAL_SECTION cs;
        /*bool cs;*/
    };

    data_t& data() { static data_t instance; return instance; }

    struct data_lock
    {
        data_lock() { /*EnterCriticalSection(&data().cs);*//*if(data().cs) throw "double lock"; data().cs=true;*/ }
        ~data_lock() { /*LeaveCriticalSection(&data().cs);*//*data().cs=false;*/ }
    };

    node_t::node_t(const void* obj, /*int*/size_t node, bool intern=false)
    : base(obj), size(node), /*mark(data().current_mark),*/ internal(intern), object(0), destroy(0)
    {}

    void node_t::kill(node_t *node)
    {
        bool internal = node->internal;
        if(node->object && node->destroy)
        node->destroy(node->object, const_cast<void*>(node->base), node->size);
        if(!internal)
        free(static_cast<void*>(node));
    }

    bool node_t::contains(const void* obj)
    {
        static const less<const void*> cmp = less<const void*>();
        if(cmp(obj, base))
        return false;
        return cmp(obj, static_cast<const char*>(base) + size);
    }

    void * node_t::operator new(size_t s)
    {
        void *res = malloc(s);
        if(res == 0)
        throw bad_alloc();
        return res;
    }

    void * node_t::operator new(__unused size_t s, __unused void *place)
    {
        return place;
    }

    void node_t::operator delete(__unused void *n)
    {
        #ifndef NDEBUG
        cerr << "Internal error -- badly written software" << endl;
        abort();
        #endif
    }

    node_t* find_node(const void* obj)
    {
        // Construct a temporary node in order to search for the object's node.
        node_t temp(obj, 0);

        // Use lower_bound to search for the "insertion point" and determine
        // if the node at this point contains the specified object.  If the
        // insertion point is at the end or does not contain the object then
        // we've failed to find the node and return an iterator to the end.
        node_vector::iterator i = lower_bound(data().new_nodes.begin(), data().new_nodes.end(), &temp, node_cmp);
        if(i == data().new_nodes.end() || !(*i)->contains(obj))
        {
            i = lower_bound(data().old_nodes.begin(), data().old_nodes.end(), &temp, node_cmp);
            if (i == data().old_nodes.end() || !(*i)->contains(obj))
            return NULL;
        }
        // Return the found iterator.
        return *i;
    }

    node_t* find_new_node(const void* obj)
    {
        // Construct a temporary node in order to search for the object's node.
        node_t temp(obj, 0);

        // Use lower_bound to search for the "insertion point" and determine
        // if the node at this point contains the specified object.  If the
        // insertion point is at the end or does not contain the object then
        // we've failed to find the node and return an iterator to the end.
        node_vector::iterator i = lower_bound(data().new_nodes.begin(), data().new_nodes.end(), &temp, node_cmp);
        if(i == data().new_nodes.end() || !(*i)->contains(obj))
        return NULL;

        // Return the found iterator.
        return *i;
    }

    node_vector::iterator find_born_node(const void* obj)
    {
        node_vector::iterator e = data().born_nodes.end();
        for(node_vector::iterator i = data().born_nodes.begin(); i!=e; ++i)
        {
            if((*i)->contains(obj))
            return i;
        }
        return e;
    }

    void get_node(const void* obj, void (*destroy)(void*, void*, size_t))
    {
        if(!obj)
        return;

        //data_lock lock; -- podwójny lock -- po co???

        node_vector::iterator i = find_born_node(obj);
        if(i != data().born_nodes.end())
        {
            node_t* node = *i;

            data().new_nodes.push_back(node);
            data().born_nodes.erase(i);
            node->mark = data().current_mark;
            node->destroy = destroy;
            node->object = const_cast<void*>(obj);
        }
    }

    void mark(ptr_vector::iterator i, ptr_queue &que)
    {
        // Mark the node associated with the pointer and then iterate
        // through all pointers contained by the object pointed to and
        // add them to the queue for later marking.
        node_t* node = i->second;
        if(node && node->mark != data().current_mark)
        {
            node->mark = data().current_mark;
            ptr_vector::iterator j = lower_bound(data().old_pointers.begin(),
            data().old_pointers.end(),
            ptr_pair(static_cast<gc::detail::pointer_base*>(const_cast<void*>(node->base)),0));
            ptr_vector::iterator end = data().old_pointers.end();
            for(; j != end; ++j)
            {
                if (node->contains(j->first))
                que.push_back(j);
                else
                break;
            }
        }
    }


    void presort_ptr_vect(ptr_vector::iterator begin, ptr_vector::iterator end)
    {
        static const less<gc::detail::pointer_base*> cmp = less<gc::detail::pointer_base*>();
        if(end-begin < 16)
        return;

        ptr_vector::iterator i = begin+((end-begin)>>1);
        ptr_pair pivot = *i;
        *i = *begin;
        *begin = pivot;
        i = begin;

        ptr_vector::iterator j = end;
        gc::detail::pointer_base* piv = pivot.first;
        ptr_pair p;

        while(i<j)
        {
            do
            {
                ++i;
            } while(i<end && cmp(i->first, piv));
            do
            {
                --j;
            } while(cmp(piv, j->first)/* j>begin && ... -- this is not needed as piv==*begin */);

            if(i<j)
            {
                p = *i;
                *i = *j;
                *j = p;
            }
        }
        *begin = *j;
        *j = pivot;

        presort_ptr_vect(begin, j);
        presort_ptr_vect(j+1, end);
    }

    void sort_ptr_vect(ptr_vector &v)
    {
        static const less<gc::detail::pointer_base*> cmp = less<gc::detail::pointer_base*>();

        ptr_vector::iterator begin = v.begin();
        ptr_vector::iterator end = v.end();

        if(end-begin < 32)
        {
            sort(begin, end);
            return;
        }

        presort_ptr_vect(begin, end);

        ptr_vector::iterator i = begin+1;
        ptr_vector::iterator j;
        ptr_pair p = *begin;
        for(j=i; j<begin+17; ++j)
        {
            if(cmp(j->first, p.first))
            p = *j;
        }
        *(begin-1) = p; // this is perfectly legal with fastvect

        while(i<end)
        {
            if(cmp(i->first, (i-1)->first))
            {
                j = i-1;
                p = *i;
                while(cmp(p.first, j->first))
                {
                    *(j+1) = *j;
                    --j;
                }
                *(j+1) = p;
            }
            ++i;
        }

        #ifndef NDEBUG
        p = *begin;
        for(i=begin+1; i<end; ++i)
        {
            //if(cmp(i->first, p.first))
            if(i->first <= p.first && p.first)
            {
                throw gc::mem_corrupt("smartpointer order lost");
                //cerr << "Order lost at " << (i-begin) << endl;
                //abort();
            }
            p = *i;
        }
        #endif
    }


    void presort_node_vect(node_vector::iterator begin, node_vector::iterator end)
    {
        if(end-begin < 16)
        return;

        node_vector::iterator i = begin+((end-begin)>>1);
        node_t *pivot = *i;
        *i = *begin;
        *begin = pivot;
        i = begin;

        node_vector::iterator j = end;
        node_t *p;

        while(i<j)
        {
            do
            {
                ++i;
            } while(i<end && node_cmp(*i, pivot));
            do
            {
                --j;
            } while(node_cmp(pivot, *j)/* j>begin && ... -- this is not needed as piv==*begin */);

            if(i<j)
            {
                p = *i;
                *i = *j;
                *j = p;
            }
        }
        *begin = *j;
        *j = pivot;

        presort_node_vect(begin, j);
        presort_node_vect(j+1, end);
    }

    void sort_node_vect(node_vector &v)
    {
        node_vector::iterator begin = v.begin();
        node_vector::iterator end = v.end();

        if(end-begin < 32)
        {
            sort(begin, end, node_cmp);
            return;
        }

        presort_node_vect(begin, end);

        node_vector::iterator i = begin+1;
        node_vector::iterator j;
        node_t *p = *begin;
        for(j=i; j<begin+17; ++j)
        {
            if(node_cmp(*j, p))
            p = *j;
        }
        *(begin-1) = p; // this is perfectly legal with fastvect

        while(i<end)
        {
            if(node_cmp(*i, *(i-1)))
            {
                j = i-1;
                p = *i;
                while(node_cmp(p, *j))
                {
                    *(j+1) = *j;
                    --j;
                }
                *(j+1) = p;
            }
            ++i;
        }
    }


    void clean_merge_ptr()
    {
        // Clean old_pointers pulling off null references then
        // sort new_pointers and merge it into old_pointers

        ptr_vector &ops = data().old_pointers;
        ptr_vector &nps = data().new_pointers;

        sort_ptr_vect(nps);

        ptr_vector::iterator o = ops.begin();
        ptr_vector::iterator end = ops.end();
        for(ptr_vector::iterator i = o; i != end; ++i)
        {
            if(i->first != 0)
            *o++ = *i;
        }
        ops.erase(o, ops.end());
        ops.resize(ops.size()+nps.size()/*, ptr_pair(0,0)*/);

        // We do our own merge, not merge algorithm, because
        // we don't want to create new old_pointers vector
        // everytime, nor we want to go for inplace_merge, as
        // it'd be slow.
        ptr_vector::reverse_iterator r = ops.rbegin()+nps.size();
        ptr_vector::reverse_iterator n = nps.rbegin();
        size_t os = ops.size();
        #ifdef _SMGC_POOR_STL
        if(os > INT_MAX)
        #else
        if(os > static_cast<size_t>(numeric_limits<int>::max()))
        #endif
        throw gc::no_space();
        int s = (int)os;
        while(r != ops.rend() && n != nps.rend())
        {
            --s;
            if(*r > *n)
            ops[s] = *r++;
            else
            ops[s] = *n++;
            ops[s].first->pos = -1-s;
        }
        while(r != ops.rend())
        {
            --s;
            ops[s] = *r++;
            ops[s].first->pos = -1-s;
        }
        while(n != nps.rend())
        {
            --s;
            ops[s] = *n++;
            ops[s].first->pos = -1-s;
        }
        #ifndef NDEBUG
        if(s != 0)
        throw gc::mem_corrupt("internal error in ptr_vectors merge");
        void *old = 0;
        end = ops.end();
        for(ptr_vector::iterator c = ops.begin(); c != end; ++c)
        {
            ASSERT_PTR_VALID(*(c->first));
            if(c->first == old)
            throw gc::mem_corrupt("duplicated pointer");
            else if(c->first < old)
            throw gc::mem_corrupt("pointer order lost");
            old = c->first;
        }
        #endif
        nps.clear();
    }

    void clean_merge_nodes(size_t new_sweeped_out)
    {
        // Clean old_nodes pulling off null references then
        // merge old_nodes into new_nodes

        node_vector &onv = data().old_nodes;
        node_vector &nnv = data().new_nodes;

        node_vector::iterator o = onv.begin();
        node_vector::iterator end = onv.end();
        for(node_vector::iterator i = o; i != end; ++i)
        {
            if(*i != 0)
            *o++ = *i;
        }
        onv.erase(o, onv.end());
        onv.resize(onv.size()+nnv.size()-new_sweeped_out);

        // We do our own merge, not merge algorithm, because
        // we don't want to create new old_pointers vector
        // everytime, nor we want to go for inplace_merge, as
        // it'd be slow.
        node_vector::reverse_iterator r = onv.rbegin()+(nnv.size()-new_sweeped_out);
        node_vector::reverse_iterator n = nnv.rbegin();
        size_t s = onv.size();
        while(r != onv.rend() && n != nnv.rend())
        {
            if(*n == 0)
            {
                n++;
                continue;
            }
            --s;
            if(node_cmp(*n, *r))
            onv[s] = *r++;
            else
            onv[s] = *n++;
        }
        while(r != onv.rend())
        {
            --s;
            onv[s] = *r++;
        }
        while(n != nnv.rend())
        {
            if(*n == 0)
            {
                n++;
                continue;
            }
            --s;
            onv[s] = *n++;
        }
        #ifndef NDEBUG
        if(s != 0)
        throw gc::mem_corrupt("internal error in node_sets merge");
        node_t *old = 0;
        end = onv.end();
        for(node_vector::iterator c = onv.begin(); c != end; ++c)
        {
            if(*c == old)
            throw gc::mem_corrupt("duplicated node");
            else if(old && !node_cmp(old, *c))
            throw gc::mem_corrupt("node order lost");
            old = *c;
        }
        #endif
        nnv.clear();
    }

    template<class A>
    void* gnew(size_t size, const A& a)
    {
        data_lock lock;

        bool collected = false;

        if (data().threshold != gc::no_threshold)
        {
            // Determine if we've exceeded the threshold and so should collect.
            data().allocated += size;
            if (data().allocated > data().dynamic_threshold)
            {
                gc::collect();
                collected = true;
            }
        }

        // Attempt the first allocation.  The standard requires new to throw
        // on failure but user code may change this behavior and VC++ by default
        // only returns 0.  We'll catch exceptions and if we've already collected
        // re-throw the exception.
        void* obj = 0;
        try { obj = a.alloc(size); } catch(bad_alloc&) { if (collected) throw; }

        // If we've failed to allocate but new didn't throw an exception and we've
        // not collected yet we'll collect and then re-try calling new.  If new
        // throws at this point we'll ignore it and let the caller handle it.
        if (obj == 0 && !collected)
        {
            gc::collect();
            obj = a.alloc(size);
        }

        // If we actually allocated memory then we need to add it to the node map.
        try
        {
            if (obj != 0)
            {
                node_t *node;
                if(a.internal_node())
                node = new(static_cast<void*>(static_cast<char*>(obj)+a.node_offset(size))) node_t(obj, size, true);
                else
                node = new node_t(obj, size);
                if(node == 0) // Dealing with VC++ 6.0 quirk
                throw gc::bad_alloc();
                data().born_nodes.push_back(node);
            }
        }
        catch (...)
        {
            // If inserting into the map failed clean up and simply throw
            // a bad_alloc exception.
            a.free(obj);
            throw gc::bad_alloc();
        }

        return obj;
    }

    template<class A>
    void gfree(void* obj, const A &a)
    {
        data_lock lock;

        // Theoretically, none of this code will throw an exception.
        // If an exception occurs the best we can do is assume that
        // everything worked any way and ignore the exception.
        try
        {
            node_vector::iterator i = find_born_node(obj);
            node_t* node = *i;
            if (node)
            data().born_nodes.erase(i);

            // This operator really should only be called when
            // construction of an object fails, in which case
            // there won't be a "registered destructor" and the
            // following will only delete the node.
            node_t::kill(node);
        }
        catch (...)
        {
        }

        // Because there was no "registered destructor" we'll still
        // need to delete the memory allocated by operator new(GC).
        a.free(obj);
    }

    struct alloc_single_t
    {
        static void *alloc(size_t s)
        {
            return ::operator new(node_offset(s) + sizeof(node_t));
        }

        static void free(void *obj)
        {
            ::operator delete(obj);
        }

        static size_t node_offset(size_t s)
        {
            size_t r = ((s+sizeof(void(*)())-1) / sizeof(void(*)())) * sizeof(void(*)());
            if(r>2000000000u) abort();
            return r;
        }

        static bool internal_node()
        {
            return true;
        }
    };

    alloc_single_t alloc_single;

    #ifndef _SMGC_NO_ARRAY_NEW
    struct alloc_array_t
    {
        static void *alloc(size_t s)
        {
            return ::operator new[](s);
        }

        static void free(void *obj)
        {
            ::operator delete[](obj);
        }

        static size_t node_offset(__unused size_t s)
        {
            return 0;
        }

        static bool internal_node()
        {
            return false;
        }
    };

    alloc_array_t alloc_array;
    #endif

}


namespace gc
{
    namespace detail
    {
        pointer_base::pointer_base()
        {
            data_lock lock;

            #ifndef NDEBUG
            destroyed = false;
            # ifdef DEBUG_COMPILER
            trash = new char[1];
            # endif
            #endif
            ptr_vector &ps = data().new_pointers;
            pos = ps.size();
            ps.push_back(ptr_pair(this, 0));
        }

        pointer_base::pointer_base(__unused const pointer_base &pb)
        {
            data_lock lock;

            #ifndef NDEBUG
            destroyed = false;
            # ifdef DEBUG_COMPILER
            trash = new char[3];
            # endif
            #endif
            ptr_vector &ps = data().new_pointers;
            pos = ps.size();
            /*if(pb.pos >= 0)
            ps.push_back(ptr_pair(this, ps[pb.pos].second));
            else
            ps.push_back(ptr_pair(this, data().old_pointers[-1-pb.pos].second));
            */
            ps.push_back(ptr_pair(this, 0));
        }

        pointer_base::~pointer_base()
        {
            data_lock lock;

            #ifndef NDEBUG
            # ifdef DEBUG_COMPILER
            delete[] trash;
            # endif
            destroyed = true;
            #endif
            if(pos >= 0)
            {
                ptr_vector &ps = data().new_pointers;
                if(static_cast<size_t>(pos) == ps.size()-1)
                {
                    ps.pop_back();
                }
                else
                {
                    ps[pos] = ps.back();
                    #ifndef NDEBUG
                    if(static_cast<size_t>(ps[pos].first->pos) != ps.size()-1)
                    {
                        //cerr << "GC: memory corrupted; smartpointer lost!" << endl;
                        //abort();
                        throw gc::mem_corrupt("smartpointer lost");
                    }
                    if(ps[pos].first->destroyed)
                    {
                        //cerr << "GC: memory corrupted; smartpointer destructed 2 times or overwritten!" << endl;
                        //abort();
                        throw gc::mem_corrupt("smartpointer overwritten or double destructed");
                    }
                    #endif
                    ps[pos].first->pos = pos;
                    ps.pop_back();
                }
            }
            else
            {
                data().old_pointers[-1-pos].first = 0;
            }
        }

        void pointer_base::reset_node(const void* obj, void (*destroy)(void*, void*, size_t))
        {
            data_lock lock;
            get_node(obj, destroy);
        }

        weak_base::weak_base()
        {
            data_lock lock;

            #ifndef NDEBUG
            destroyed = false;
            # ifdef DEBUG_COMPILER
            trash = new char[2];
            # endif
            #endif
            weak_vector &ps = data().weak_pointers;
            pos = ps.size();
            ps.push_back(weak_pair(this, 0));
        }

        weak_base::weak_base(__unused const weak_base &pb)
        {
            data_lock lock;

            #ifndef NDEBUG
            destroyed = false;
            # ifdef DEBUG_COMPILER
            trash = new char[4];
            # endif
            #endif
            weak_vector &ps = data().weak_pointers;
            pos = ps.size();
            ps.push_back(weak_pair(this, 0));
        }

        weak_base::~weak_base()
        {
            data_lock lock;

            #ifndef NDEBUG
            # ifdef DEBUG_COMPILER
            delete[] trash;
            # endif
            destroyed = true;
            #endif
            weak_vector &ps = data().weak_pointers;
            if(static_cast<size_t>(pos) == ps.size()-1)
            {
                ps.pop_back();
            }
            else
            {
                ps[pos] = ps.back();
                #ifndef NDEBUG
                if(static_cast<size_t>(ps[pos].first->pos) != ps.size()-1)
                {
                    throw gc::mem_corrupt("weakpointer lost");
                }
                if(ps[pos].first->destroyed)
                {
                    throw gc::mem_corrupt("weakpointer overwritten or double destructed");
                }
                #endif
                ps[pos].first->pos = pos;
                ps.pop_back();
            }
        }


        // Register memory area in GC. Area must be freeable using supplied destroy function
        void reg(void *obj, size_t size, void (*destroy)(void*, void*, size_t))
        {
            data_lock lock;

            __unused bool collected = false;

            if (data().threshold != gc::no_threshold)
            {
                // Determine if we've exceeded the threshold and so should collect.
                data().allocated += size;
                if (data().allocated > data().dynamic_threshold)
                {
                    gc::collect();
                    collected = true;
                }
            }

            // If we actually allocated memory then we need to add it to the node map.
            try
            {
                if (obj != 0)
                {
                    node_t *node = new node_t(obj, size);
                    if(node == 0)
                    throw gc::bad_alloc();
                    data().new_nodes.push_back(node);
                    node->destroy = destroy;
                    node->object = const_cast<void*>(obj);
                }
            }
            catch (...)
            {
                // If inserting into the map failed simply throw
                // a bad_reg exception.
                throw gc::bad_reg();
            }
        }

    }

    bad_reg::~bad_reg() throw() {}
    const char* bad_reg::what() const throw()
    {
        return "unable to register pointer in GC";
    }

    no_space::~no_space() throw() {}
    const char* no_space::what() const throw()
    {
        return "out of pointer space";
    }

    mem_corrupt::mem_corrupt(const char *txt)
    {
        msg = new char[strlen("GC detected memory corruption: ")+strlen(txt)+1];
        strcpy(msg, "GC detected memory corruption: ");
        strcat(msg, txt);
    }
    mem_corrupt::~mem_corrupt() throw() { delete[] msg; }
    const char* mem_corrupt::what() const throw()
    {
        return msg;
    }

    #if (defined(_MSC_VER) && (_MSVC <= 1200)) || defined(_SMGC_POOR_STL)
    const size_t no_threshold = ~(static_cast<size_t>(0));
    #else
    const size_t no_threshold = numeric_limits<size_t>::max();
    #endif

    const size_t default_threshold = (INT_MAX>=4194304*8)?4194304:(INT_MAX/8);


    void collect()
    {
        data_lock lock;

        // During the sweep phase we'll be deleting objects that could cause
        // a recursive call to 'collect' which would cause invalid results.  So
        // we prevent recursion here.
        if (data().collecting)
        return;

        data().collecting = true;

        #ifndef NDEBUG
        //cerr << "Allocated: " << data().allocated << endl;
        #endif

        // Toggle the 'current_mark' so that we can start over.
        data().current_mark = !data().current_mark;
        bool current_mark = data().current_mark;

        { // Mark phase
            // Loop through all of the pointers looking for 'root' pointers.  A 'root'
            // pointer is a pointer that's not contained within the object pointed
            // to by any other pointer.  When a 'root' pointer is found it is
            // marked, and all the pointers referenced through the 'root' pointer
            // are also marked.
            clean_merge_ptr();
            //sort(data().new_nodes.begin(), data().new_nodes.end(), node_cmp);
            sort_node_vect(data().new_nodes);

            ptr_vector::iterator end = data().old_pointers.end();
            for(ptr_vector::iterator i = data().old_pointers.begin(); i != end; ++i)
            {
                gc::detail::pointer_base* ptr = i->first;
                if(!(reinterpret_cast<gc_ptr<void*>& >(*ptr)).ptr)
                {
                    i->second = 0;
                    continue;
                }
                node_t* node = i->second;
                if(!node || !node->contains((reinterpret_cast<gc_ptr<void*>& >(*ptr)).ptr))
                {
                    node = find_node((reinterpret_cast<gc_ptr<void*>& >(*ptr)).ptr);
                    //#ifndef NDEBUG
                    //          if(!node)
                    //          {
                        //            //cerr << "Pointer points to nonsensical address" << endl;
                        //            //abort();
                        //            throw gc::mem_corrupt("smartpointer points to nonsense address");
                    //          }
                    //#endif
                    i->second = node;
                }
            }
            ptr_queue que;
            for(ptr_vector::iterator ii = data().old_pointers.begin(); ii != end; ++ii)
            {
                node_t* node = ii->second;
                if (!node || node->mark == current_mark)
                continue; // Don't need to check pointers that are marked.

                // If we can't find a node that contains the pointer it's a root pointer
                // and should be marked.
                node = find_node(ii->first);
                if (!node)
                que.push_back(ii);
            }
            while(que.size())
            {
                mark(que.front(), que);
                que.pop_front();
            }
            // Nullify weak pointers pointing to unmarked nodes
            weak_vector::iterator weak_end = data().weak_pointers.end();
            for(weak_vector::iterator j = data().weak_pointers.begin(); j != weak_end; ++j)
            {
                gc::detail::weak_base* ptr = j->first;
                if(!(reinterpret_cast<wk_ptr<void*>& >(*ptr)).ptr)
                {
                    j->second = 0;
                    continue;
                }
                node_t* node = j->second;
                if(!node || !node->contains((reinterpret_cast<wk_ptr<void*>& >(*ptr)).ptr))
                {
                    node = find_node((reinterpret_cast<wk_ptr<void*>& >(*ptr)).ptr);
                    j->second = node;
                }
                if(!node || node->mark != current_mark)
                reinterpret_cast<wk_ptr<void*>& >(*ptr).ptr = 0;
            }
        }

        { // Sweep phase
            // Step through all of the nodes and delete any that are not marked.
            node_vector::iterator i;
            node_vector::iterator end = data().new_nodes.end();
            size_t nnsweep = 0;
            size_t freed = 0;
            size_t remain = 0;
            for (i = data().new_nodes.begin(); i != end; /*nothing*/)
            {
                node_t* node = *i;
                remain += node->size;
                if (node->mark != current_mark)
                {
                    if (node->destroy == 0)
                    {
                        // We must be constructing this object, so we'll just mark it.
                        // This prevents premature collection of objects that call
                        // gc_collect during the construction phase before any gc_ptr<>'s
                        // are assigned to the object.
                        node->mark = current_mark;
                    }
                    else
                    {
                        ++nnsweep;
                        freed += node->size;
                        *i = 0;
                        node_t::kill(node);
                    }
                }
                ++i;
            }
            end = data().old_nodes.end();
            for (i = data().old_nodes.begin(); i != end; /*nothing*/)
            {
                node_t* node = *i;
                remain += node->size;
                if (node->mark != current_mark)
                {
                    if (node->destroy == 0)
                    {
                        // We must be constructing this object, so we'll just mark it.
                        // This prevents premature collection of objects that call
                        // gc_collect during the construction phase before any gc_ptr<>'s
                        // are assigned to the object.
                        node->mark = current_mark;
                    }
                    else
                    {
                        freed += node->size;
                        *i = 0;
                        node_t::kill(node);
                    }
                }
                ++i;
            }
            remain -= freed;

            clean_merge_nodes(nnsweep);

            if(data().threshold != gc::no_threshold && data().threshold > 0)
            {
                if(data().dynamic_threshold < freed/2 && data().dynamic_threshold > data().threshold/2)
                {
                    data().dynamic_threshold /= 2;
                }
                else if(data().dynamic_threshold < freed/2 && data().dynamic_threshold > data().threshold/4)
                {
                    data().dynamic_threshold = data().threshold/4;
                }
                else if(data().dynamic_threshold > remain && data().dynamic_threshold > data().threshold)
                {
                    data().dynamic_threshold /= 4;
                }
                else if(data().dynamic_threshold > remain && data().dynamic_threshold > data().threshold/4)
                {
                    data().dynamic_threshold = data().threshold/4;
                }
                else if(data().dynamic_threshold < remain/4)
                {
                    data().dynamic_threshold *= 2;
                }
                else if(data().dynamic_threshold > freed/16)
                {
                    data().dynamic_threshold *= 4;
                }
                else if(data().dynamic_threshold > freed/4)
                {
                    data().dynamic_threshold *= 2;
                }
            }
        }

        data().collecting = false;
        data().allocated = 0;
    }

    void set_threshold(size_t bytes)
    {
        data_lock lock;
        data().threshold = bytes;
        data().dynamic_threshold = bytes;
    }

    size_t get_threshold()
    {
        data_lock lock;
        return data().threshold;
    }

    size_t get_dynamic_threshold()
    {
        data_lock lock;
        return data().dynamic_threshold;
    }
}


#ifndef _SMGC_NO_PLACEMENT_NEW
void* operator new(size_t s, const gc::detail::gc_t&) throw (gc::bad_alloc)
{
    return gnew(s, alloc_single);
}

void operator delete(void *obj, const gc::detail::gc_t&) throw ()
{
    gfree(obj, alloc_single);
}

#  ifndef _SMGC_NO_ARRAY_NEW
void* operator new[](size_t s, const gc::detail::gc_t&) throw (gc::bad_alloc)
{
    return gnew(s, alloc_array);
}

void operator delete[](void *obj, const gc::detail::gc_t&) throw ()
{
    gfree(obj, alloc_array);
}
#  endif
#endif
