constexpr gc_ptr() : base_gc_ptr(T)() {}

gc_ptr(T* p) : base_gc_ptr(T)(p) {}

template< class Y >
explicit gc_ptr( Y* ptr ) : base_gc_ptr(Y)(ptr) {}

template< class Y, class Deleter >
gc_ptr( Y* ptr, Deleter d ) : base_gc_ptr(Y)(ptr, d) {}

template< class Y, class Deleter, class Alloc >
gc_ptr( Y* ptr, Deleter d, Alloc alloc ) : base_gc_ptr(Y)(ptr, d, alloc) {}

constexpr gc_ptr( std::nullptr_t ) : base_gc_ptr(T)(nullptr) {}

template< class Deleter >
gc_ptr( std::nullptr_t, Deleter d ) : base_gc_ptr(T)(nullptr, d) {}

template< class Deleter, class Alloc >
gc_ptr( std::nullptr_t, Deleter d, Alloc alloc ) : base_gc_ptr(T)(nullptr, d, alloc) {}

template< class Y >
gc_ptr( const gc_ptr<Y>& r, T *ptr ) : base_gc_ptr(Y)(r, ptr) {}

gc_ptr( const gc_ptr& r ) : base_gc_ptr(T)(r) {}

template< class Y >
gc_ptr( const gc_ptr<Y>& r ) : base_gc_ptr(Y)(r) {}

gc_ptr( gc_ptr&& r ) : base_gc_ptr(T)(r) {}

template< class Y >
gc_ptr( gc_ptr<Y>&& r ) : base_gc_ptr(Y)(r) {}

template< class Y >
explicit gc_ptr( const std::weak_ptr<Y>& r ) : base_gc_ptr(Y)(r) {}

template< class Y >
gc_ptr( std::auto_ptr<Y>&& r ) : base_gc_ptr(Y)(r) {}

template< class Y, class Deleter >
gc_ptr( std::unique_ptr<Y,Deleter>&& r ) : base_gc_ptr(Y)(r) {}

