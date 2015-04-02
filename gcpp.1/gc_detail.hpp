#ifndef GC_DETAIL_HPP
#define GC_DETAIL_HPP
namespace gc {
    template<typename T>
    struct gc_detail {
        static const
        gc_id_t NOT_REGISTERED = 0;
        bool _disposed =  false;
        T type;
        gc_detail(
            T* data,
            gc_id_t register_id,
            bool is_stack = true,
            bool disposed = false)
            :   _disposed(disposed),
                _data(data),
                _is_stack(is_stack),
                _register_id(register_id) { }
        inline T* get_data() const { return this->_data; }
        inline bool stack_referred() const { return this->_is_stack; }
        inline gc_id_t get_id() const { return this->_register_id; }
        inline bool has_disposed() const { return this->_disposed; }
        inline void make_disposed(bool check = true) { this->_disposed = check; }
    private:
        T* _data = nullptr;
        bool _is_stack = false;
        gc_id_t _register_id =  NOT_REGISTERED;
    };
}
#endif // GC_DETAIL_HPP
