gc_ptr<T>& operator = (const nullptr_t&)
{
    this->reset(nullptr);
    return *this;
}

template<typename U>
gc_ptr<T>& operator = (U *p)
{
    this->reset(p);
    return *this;
}

template<typename U>
gc_ptr<T>& operator = (const gc_ptr<U> &p)
{
    this->swap(p);
    return *this;
}
