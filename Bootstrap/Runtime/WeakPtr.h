/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeWeakPtr.h>
#else

#    include "Weakable.h"

template<typename T>
class [[nodiscard]] WeakPtr {

    template<typename U>
    friend class Weakable;

public:

    WeakPtr() = default;

    // Someone decided that `WeakPtr<T>` should be constructible from `None` in Jakt.
    WeakPtr(NullOptional) {}

    template<typename U>
    WeakPtr(WeakPtr<U> const& other) requires(IsBaseOf<T, U>)
        : m_link(other.m_link) { }

    template<typename U>
    WeakPtr(WeakPtr<U>&& other) requires(IsBaseOf<T, U>)
        : m_link(other.take_link()) { }

    template<typename U>
    WeakPtr& operator=(WeakPtr<U>&& other) requires(IsBaseOf<T, U>) {

        m_link = other.take_link();

        return *this;
    }

    template<typename U>
    WeakPtr& operator=(WeakPtr<U> const& other) requires(IsBaseOf<T, U>) {

        if ((void const*)this != (void const*)&other) {

            m_link = other.m_link;
        }

        return *this;
    }

    WeakPtr& operator=(std::nullptr_t) {

        clear();
        
        return *this;
    }

    template<typename U>
    WeakPtr(const U& object) requires(IsBaseOf<T, U>)
        : m_link(object.template make_weak_ptr<U>().take_link()) { }

    template<typename U>
    WeakPtr(const U* object) requires(IsBaseOf<T, U>) {

        if (object) {

            m_link = object->template make_weak_ptr<U>().take_link();
        }
    }

    template<typename U>
    WeakPtr(RefPtr<U> const& object) requires(IsBaseOf<T, U>)
    {
        if (object)
            m_link = object->template make_weak_ptr<U>().take_link();
    }

    template<typename U>
    WeakPtr(NonnullRefPtr<U> const& object) requires(IsBaseOf<T, U>)
    {
        m_link = object->template make_weak_ptr<U>().take_link();
    }

    template<typename U>
    WeakPtr& operator=(const U& object) requires(IsBaseOf<T, U>)
    {
        m_link = object.template make_weak_ptr<U>().take_link();
        return *this;
    }

    template<typename U>
    WeakPtr& operator=(const U* object) requires(IsBaseOf<T, U>)
    {
        if (object)
            m_link = object->template make_weak_ptr<U>().take_link();
        else
            m_link = nullptr;
        return *this;
    }

    template<typename U>
    WeakPtr& operator=(RefPtr<U> const& object) requires(IsBaseOf<T, U>)
    {
        if (object)
            m_link = object->template make_weak_ptr<U>().take_link();
        else
            m_link = nullptr;
        return *this;
    }

    template<typename U>
    WeakPtr& operator=(NonnullRefPtr<U> const& object) requires(IsBaseOf<T, U>)
    {
        m_link = object->template make_weak_ptr<U>().take_link();
        return *this;
    }

    [[nodiscard]] RefPtr<T> strong_ref() const
    {
        return RefPtr<T> { ptr() };
    }

    T* ptr() const { return unsafe_ptr(); }
    T* operator->() { return unsafe_ptr(); }
    const T* operator->() const { return unsafe_ptr(); }
    operator const T*() const { return unsafe_ptr(); }
    operator T*() { return unsafe_ptr(); }

    [[nodiscard]] T* unsafe_ptr() const
    {
        if (m_link)
            return m_link->template unsafe_ptr<T>();
        return nullptr;
    }

    operator bool() const { return m_link ? !m_link->isNull() : false; }

    [[nodiscard]] bool isNull() const { return !m_link || m_link->isNull(); }

    [[nodiscard]] bool hasValue() const { return !isNull(); }
    T* value() { return ptr(); }
    T const* value() const { return ptr(); }

    void clear() { m_link = nullptr; }

    [[nodiscard]] RefPtr<WeakLink> take_link() { return move(m_link); }

private:

    WeakPtr(RefPtr<WeakLink> const& link)
        : m_link(link) { }

    RefPtr<WeakLink> m_link;
};

template<typename T>
template<typename U>
inline ErrorOr<WeakPtr<U>> Weakable<T>::tryMakeWeakPointer() const
{
    if (!m_link)
        m_link = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) WeakLink(const_cast<T&>(static_cast<T const&>(*this)))));

    return WeakPtr<U>(m_link);
}

template<typename T>
struct Formatter<WeakPtr<T>> : Formatter<const T*> {

    ErrorOr<void> format(FormatBuilder& builder, WeakPtr<T> const& value) {

        return Formatter<const T*>::format(builder, value.ptr());
    }
};

template<typename T>
ErrorOr<WeakPtr<T>> tryMakeWeakPointerIfNonNull(T const* ptr) {

    if (ptr) {

        return ptr->template tryMakeWeakPointer<T>();
    }
    
    return WeakPtr<T> {};
}

template<typename T>
WeakPtr<T> make_weak_ptr_if_nonnull(T const* ptr) {

    return MUST(tryMakeWeakPointerIfNonNull(ptr));
}

#endif
