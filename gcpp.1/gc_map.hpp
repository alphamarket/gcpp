#ifndef HDR__GC_MAP_HPP
#define HDR__GC_MAP_HPP
#include <unordered_map>
namespace gc {
    template<typename T>                                class gc_ptr;
    typedef intptr_t                                    gc_id_t;
    class gc_map {
        template<typename T> friend class               gc_ptr;
        typedef std::unordered_map<gc_id_t, size_t>     map_t;
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
        inline static size_t ref_up(const gc_id_t& p) {
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
        inline static size_t ref_down(const gc_id_t& p) {
            if(p == 0 || !gc_map::instance()._gc_map.count(p)) return -1;
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
}
#endif // GC_MAP_HPP
