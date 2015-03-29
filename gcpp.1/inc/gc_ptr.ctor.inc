#define __prevent_del [](T*)->void{}

gc_ptr() : base_gc_ptr(T)(nullptr) { }

template<typename U>
explicit gc_ptr(U* p) : base_gc_ptr(T)(p) { }

gc_ptr(T* p) : base_gc_ptr(T)(p) { }

template<typename _T, typename _Delete>
gc_ptr(_T* p, _Delete d) : base_gc_ptr(_T)(p, d) {}

#undef __prevent_del

