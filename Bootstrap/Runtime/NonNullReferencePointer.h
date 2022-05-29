/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define NONNULLREFPTR_SCRUB_BYTE 0xe1

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeNonNullReferencePointer.h>
#else
#    include "Assertions.h"
#    include "Atomic.h"
#    include "Format.h"
#    include "Traits.h"
#    include "Types.h"

template<typename T>
class RefPtr;

template<typename T>
ALWAYS_INLINE void refIfNotNull(T* ptr) {

    if (ptr) {

        ptr->ref();
    }
}

template<typename T>
ALWAYS_INLINE void unrefIfNotNull(T* ptr) {

    if (ptr) {

        ptr->unref();
    }
}

template<typename T>
class [[nodiscard]] NonNullReferencePointer {

    template<typename U>
    friend class RefPtr;

    template<typename U>
    friend class NonNullReferencePointer;

    template<typename U>
    friend class WeakPtr;

public:

    using ElementType = T;

    enum AdoptTag { Adopt };

    ALWAYS_INLINE NonNullReferencePointer(T const& object)
        : m_ptr(const_cast<T*>(&object)) {

        m_ptr->ref();
    }

    template<typename U>
    ALWAYS_INLINE NonNullReferencePointer(U const& object) requires(IsConvertible<U*, T*>)
        : m_ptr(const_cast<T*>(static_cast<T const*>(&object)))
    {
        m_ptr->ref();
    }

    ALWAYS_INLINE NonNullReferencePointer(AdoptTag, T& object)
        : m_ptr(&object)
    {
    }

    ALWAYS_INLINE NonNullReferencePointer(NonNullReferencePointer&& other)
        : m_ptr(&other.leak_ref())
    {
    }

    template<typename U>
    ALWAYS_INLINE NonNullReferencePointer(NonNullReferencePointer<U>&& other) requires(IsConvertible<U*, T*>)
        : m_ptr(static_cast<T*>(&other.leak_ref()))
    {
    }

    ALWAYS_INLINE NonNullReferencePointer(NonNullReferencePointer const& other)
        : m_ptr(const_cast<T*>(other.ptr()))
    {
        m_ptr->ref();
    }

    template<typename U>
    ALWAYS_INLINE NonNullReferencePointer(NonNullReferencePointer<U> const& other) requires(IsConvertible<U*, T*>)
        : m_ptr(const_cast<T*>(static_cast<T const*>(other.ptr())))
    {
        m_ptr->ref();
    }

    ALWAYS_INLINE ~NonNullReferencePointer()
    {
        unrefIfNotNull(m_ptr);
        m_ptr = nullptr;
#    ifdef SANITIZE_PTRS
        m_ptr = reinterpret_cast<T*>(explode_byte(NONNULLREFPTR_SCRUB_BYTE));
#    endif
    }

    template<typename U>
    NonNullReferencePointer(RefPtr<U> const&) = delete;
    template<typename U>
    NonNullReferencePointer& operator=(RefPtr<U> const&) = delete;
    NonNullReferencePointer(RefPtr<T> const&) = delete;
    NonNullReferencePointer& operator=(RefPtr<T> const&) = delete;

    NonNullReferencePointer& operator=(NonNullReferencePointer const& other)
    {
        NonNullReferencePointer tmp { other };
        swap(tmp);
        return *this;
    }

    template<typename U>
    NonNullReferencePointer& operator=(NonNullReferencePointer<U> const& other) requires(IsConvertible<U*, T*>)
    {
        NonNullReferencePointer tmp { other };
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE NonNullReferencePointer& operator=(NonNullReferencePointer&& other)
    {
        NonNullReferencePointer tmp { move(other) };
        swap(tmp);
        return *this;
    }

    template<typename U>
    NonNullReferencePointer& operator=(NonNullReferencePointer<U>&& other) requires(IsConvertible<U*, T*>)
    {
        NonNullReferencePointer tmp { move(other) };
        swap(tmp);
        return *this;
    }

    NonNullReferencePointer& operator=(T const& object)
    {
        NonNullReferencePointer tmp { object };
        swap(tmp);
        return *this;
    }

    [[nodiscard]] ALWAYS_INLINE T& leak_ref()
    {
        T* ptr = exchange(m_ptr, nullptr);
        VERIFY(ptr);
        return *ptr;
    }

    ALWAYS_INLINE RETURNS_NONNULL T* ptr()
    {
        return as_nonnull_ptr();
    }
    ALWAYS_INLINE RETURNS_NONNULL const T* ptr() const
    {
        return as_nonnull_ptr();
    }

    ALWAYS_INLINE RETURNS_NONNULL T* operator->()
    {
        return as_nonnull_ptr();
    }
    ALWAYS_INLINE RETURNS_NONNULL const T* operator->() const
    {
        return as_nonnull_ptr();
    }

    ALWAYS_INLINE T& operator*()
    {
        return *as_nonnull_ptr();
    }
    ALWAYS_INLINE const T& operator*() const
    {
        return *as_nonnull_ptr();
    }

    ALWAYS_INLINE RETURNS_NONNULL operator T*()
    {
        return as_nonnull_ptr();
    }
    ALWAYS_INLINE RETURNS_NONNULL operator const T*() const
    {
        return as_nonnull_ptr();
    }

    ALWAYS_INLINE operator T&()
    {
        return *as_nonnull_ptr();
    }
    ALWAYS_INLINE operator const T&() const
    {
        return *as_nonnull_ptr();
    }

    operator bool() const = delete;
    bool operator!() const = delete;

    void swap(NonNullReferencePointer& other)
    {
        ::swap(m_ptr, other.m_ptr);
    }

    template<typename U>
    void swap(NonNullReferencePointer<U>& other) requires(IsConvertible<U*, T*>)
    {
        ::swap(m_ptr, other.m_ptr);
    }

    // clang-format off
private:
    NonNullReferencePointer() = delete;
    // clang-format on

    ALWAYS_INLINE RETURNS_NONNULL T* as_nonnull_ptr() const
    {
        VERIFY(m_ptr);
        return m_ptr;
    }

    T* m_ptr { nullptr };
};

template<typename T>
inline NonNullReferencePointer<T> adopt_ref(T& object)
{
    return NonNullReferencePointer<T>(NonNullReferencePointer<T>::Adopt, object);
}

template<typename T, typename U>
inline void swap(NonNullReferencePointer<T>& a, NonNullReferencePointer<U>& b) requires(IsConvertible<U*, T*>)
{
    a.swap(b);
}

template<typename T, class... Args>
requires(IsConstructible<T, Args...>) inline NonNullReferencePointer<T> make_ref_counted(Args&&... args) {

    return NonNullReferencePointer<T>(NonNullReferencePointer<T>::Adopt, *new T(forward<Args>(args)...));
}

// FIXME: Remove once P0960R3 is available in Clang.
template<typename T, class... Args>
inline NonNullReferencePointer<T> make_ref_counted(Args&&... args) {

    return NonNullReferencePointer<T>(NonNullReferencePointer<T>::Adopt, *new T { forward<Args>(args)... });
}

template<typename T>
struct Traits<NonNullReferencePointer<T>> : public GenericTraits<NonNullReferencePointer<T>> {

    using PeekType = T*;
    using ConstPeekType = const T*;
    static unsigned hash(NonNullReferencePointer<T> const& p) { return ptr_hash(p.ptr()); }
    static bool equals(NonNullReferencePointer<T> const& a, NonNullReferencePointer<T> const& b) { return a.ptr() == b.ptr(); }
};

#endif
