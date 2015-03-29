gc_ptr() : base(nullptr) { }

template<typename U>
explicit gc_ptr(U* p) : base(p) { }

gc_ptr(T* p) : base(p) { }

template<typename _T, typename _Delete>
gc_ptr(_T* p, _Delete d) : base(p, d) {}
