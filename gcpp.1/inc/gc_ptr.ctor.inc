gc_ptr() : base(nullptr) { }

template<typename U>
explicit gc_ptr(U* p) : base(p) { }

gc_ptr(T* p) : base(p) { }
