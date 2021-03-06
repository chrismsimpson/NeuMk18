/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeWeakPointer.h>
#else

#    include "Weakable.h"

template<typename T>
class [[nodiscard]] WeakPointer {

    template<typename U>
    friend class Weakable;

public:

    WeakPointer() = default;

    // Someone decided that `WeakPointer<T>` should be constructible from `None` in Jakt.
    WeakPointer(NullOptional) {}

    template<typename U>
    WeakPointer(WeakPointer<U> const& other) requires(IsBaseOf<T, U>)
        : m_link(other.m_link) { }

    template<typename U>
    WeakPointer(WeakPointer<U>&& other) requires(IsBaseOf<T, U>)
        : m_link(other.takeLink()) { }

    template<typename U>
    WeakPointer& operator=(WeakPointer<U>&& other) requires(IsBaseOf<T, U>) {

        m_link = other.takeLink();

        return *this;
    }

    template<typename U>
    WeakPointer& operator=(WeakPointer<U> const& other) requires(IsBaseOf<T, U>) {

        if ((void const*)this != (void const*)&other) {

            m_link = other.m_link;
        }

        return *this;
    }

    WeakPointer& operator=(std::nullptr_t) {

        clear();
        
        return *this;
    }

    template<typename U>
    WeakPointer(const U& object) requires(IsBaseOf<T, U>)
        : m_link(object.template makeWeakPointer<U>().takeLink()) { }

    template<typename U>
    WeakPointer(const U* object) requires(IsBaseOf<T, U>) {

        if (object) {

            m_link = object->template makeWeakPointer<U>().takeLink();
        }
    }

    template<typename U>
    WeakPointer(ReferencePointer<U> const& object) requires(IsBaseOf<T, U>) {

        if (object) {

            m_link = object->template makeWeakPointer<U>().takeLink();
        }
    }

    template<typename U>
    WeakPointer(NonNullReferencePointer<U> const& object) requires(IsBaseOf<T, U>) {

        m_link = object->template makeWeakPointer<U>().takeLink();
    }

    template<typename U>
    WeakPointer& operator=(const U& object) requires(IsBaseOf<T, U>) {

        m_link = object.template makeWeakPointer<U>().takeLink();
        
        return *this;
    }

    template<typename U>
    WeakPointer& operator=(const U* object) requires(IsBaseOf<T, U>) {

        if (object)
            m_link = object->template makeWeakPointer<U>().takeLink();
        else
            m_link = nullptr;
        return *this;
    }

    template<typename U>
    WeakPointer& operator=(ReferencePointer<U> const& object) requires(IsBaseOf<T, U>) {

        if (object)
            m_link = object->template makeWeakPointer<U>().takeLink();
        else
            m_link = nullptr;
        return *this;
    }

    template<typename U>
    WeakPointer& operator=(NonNullReferencePointer<U> const& object) requires(IsBaseOf<T, U>) {

        m_link = object->template makeWeakPointer<U>().takeLink();
        
        return *this;
    }

    [[nodiscard]] ReferencePointer<T> strong_ref() const {

        return ReferencePointer<T> { pointer() };
    }

    T* pointer() const { return unsafePointer(); }
    T* operator->() { return unsafePointer(); }
    const T* operator->() const { return unsafePointer(); }
    operator const T*() const { return unsafePointer(); }
    operator T*() { return unsafePointer(); }

    [[nodiscard]] T* unsafePointer() const {

        if (m_link) {

            return m_link->template unsafePointer<T>();
        }

        return nullptr;
    }

    operator bool() const { return m_link ? !m_link->isNull() : false; }

    [[nodiscard]] bool isNull() const { return !m_link || m_link->isNull(); }

    [[nodiscard]] bool hasValue() const { return !isNull(); }
    T* value() { return pointer(); }
    T const* value() const { return ptr(); }

    void clear() { m_link = nullptr; }

    [[nodiscard]] ReferencePointer<WeakLink> takeLink() { return move(m_link); }

private:

    WeakPointer(ReferencePointer<WeakLink> const& link)
        : m_link(link) { }

    ReferencePointer<WeakLink> m_link;
};

template<typename T>
template<typename U>
inline ErrorOr<WeakPointer<U>> Weakable<T>::tryMakeWeakPointer() const {

    if (!m_link) {

        m_link = TRY(adoptNonNullReferenceOrErrorNoMemory(new (nothrow) WeakLink(const_cast<T&>(static_cast<T const&>(*this)))));
    }

    return WeakPointer<U>(m_link);
}

template<typename T>
struct Formatter<WeakPointer<T>> : Formatter<const T*> {

    ErrorOr<void> format(FormatBuilder& builder, WeakPointer<T> const& value) {

        return Formatter<const T*>::format(builder, value.pointer());
    }
};

template<typename T>
ErrorOr<WeakPointer<T>> tryMakeWeakPointerIfNonNull(T const* ptr) {

    if (ptr) {

        return ptr->template tryMakeWeakPointer<T>();
    }
    
    return WeakPointer<T> {};
}

template<typename T>
WeakPointer<T> makeWeakPointerIfNonNull(T const* ptr) {

    return MUST(tryMakeWeakPointerIfNonNull(ptr));
}

#endif
