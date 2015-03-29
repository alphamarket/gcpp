gc_ptr() : self(nullptr, self::__prevent_delete) { }

template<typename U>
explicit gc_ptr(U* p) : self(p, self::__prevent_delete) { }

gc_ptr(T* p) : self(p, self::__prevent_delete) { }

template<typename _T, typename _Delete>
gc_ptr(_T* p, _Delete d) : base(p, d) {}
