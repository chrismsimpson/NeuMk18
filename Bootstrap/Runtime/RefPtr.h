/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define REFPTR_SCRUB_BYTE 0xe0

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeRefPtr.h>
#else

#    include "Assertions.h"
#    include "Atomic.h"
#    include "Error.h"
#    include "Format.h"
#    include "NonNullReferencePointer.h"
#    include "StdLibExtras.h"
#    include "Traits.h"
#    include "Types.h"

template<typename T>
class [[nodiscard]] RefPtr {

    template<typename U>
    friend class RefPtr;

    template<typename U>
    friend class WeakPointer;

public:

    enum AdoptTag {
        Adopt
    };

    RefPtr() = default;

    RefPtr(T const* ptr)
        : m_pointer(const_cast<T*>(ptr)) {

        refIfNotNull(m_pointer);
    }

    RefPtr(T const& object)
        : m_pointer(const_cast<T*>(&object)) {

        m_pointer->ref();
    }

    RefPtr(AdoptTag, T& object)
        : m_pointer(&object) { }

    RefPtr(RefPtr&& other)
        : m_pointer(other.leak_ref()) { }

    ALWAYS_INLINE RefPtr(NonNullReferencePointer<T> const& other)
        : m_pointer(const_cast<T*>(other.pointer())) {

        m_pointer->ref();
    }

    template<typename U>
    ALWAYS_INLINE RefPtr(NonNullReferencePointer<U> const& other) requires(IsConvertible<U*, T*>)
        : m_pointer(const_cast<T*>(static_cast<T const*>(other.pointer()))) {

        m_pointer->ref();
    }

    template<typename U>
    ALWAYS_INLINE RefPtr(NonNullReferencePointer<U>&& other) requires(IsConvertible<U*, T*>)
        : m_pointer(static_cast<T*>(&other.leak_ref())) { }

    template<typename U>
    RefPtr(RefPtr<U>&& other) requires(IsConvertible<U*, T*>)
        : m_pointer(static_cast<T*>(other.leak_ref())) { }

    RefPtr(RefPtr const& other)
        : m_pointer(other.m_pointer) {

        refIfNotNull(m_pointer);
    }

    template<typename U>
    RefPtr(RefPtr<U> const& other) requires(IsConvertible<U*, T*>)
        : m_pointer(const_cast<T*>(static_cast<T const*>(other.pointer()))) {

        refIfNotNull(m_pointer);
    }

    ALWAYS_INLINE ~RefPtr()
    {
        clear();
#    ifdef SANITIZE_PTRS
        m_pointer = reinterpret_cast<T*>(explode_byte(REFPTR_SCRUB_BYTE));
#    endif
    }

    void swap(RefPtr& other)
    {
        ::swap(m_pointer, other.m_pointer);
    }

    template<typename U>
    void swap(RefPtr<U>& other) requires(IsConvertible<U*, T*>) {

        ::swap(m_pointer, other.m_pointer);
    }

    ALWAYS_INLINE RefPtr& operator=(RefPtr&& other) {

        RefPtr tmp { move(other) };
        
        swap(tmp);
        
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(RefPtr<U>&& other) requires(IsConvertible<U*, T*>) {

        RefPtr tmp { move(other) };
        swap(tmp);
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(NonNullReferencePointer<U>&& other) requires(IsConvertible<U*, T*>)
    {
        RefPtr tmp { move(other) };
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(NonNullReferencePointer<T> const& other)
    {
        RefPtr tmp { other };
        swap(tmp);
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(NonNullReferencePointer<U> const& other) requires(IsConvertible<U*, T*>)
    {
        RefPtr tmp { other };
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(RefPtr const& other)
    {
        RefPtr tmp { other };
        swap(tmp);
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE RefPtr& operator=(RefPtr<U> const& other) requires(IsConvertible<U*, T*>)
    {
        RefPtr tmp { other };
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(T const* ptr)
    {
        RefPtr tmp { ptr };
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE RefPtr& operator=(T const& object)
    {
        RefPtr tmp { object };
        swap(tmp);
        return *this;
    }

    RefPtr& operator=(std::nullptr_t)
    {
        clear();
        return *this;
    }

    ALWAYS_INLINE bool assign_if_null(RefPtr&& other) {

        if (this == &other) {

            return isNull();
        }

        *this = move(other);
        
        return true;
    }

    template<typename U>
    ALWAYS_INLINE bool assign_if_null(RefPtr<U>&& other) {

        if (this == &other) {

            return isNull();
        }
        
        *this = move(other);
        
        return true;
    }

    ALWAYS_INLINE void clear()
    {
        dereferenceIfNotNull(m_pointer);
        m_pointer = nullptr;
    }

    bool operator!() const { return !m_pointer; }

    [[nodiscard]] T* leak_ref()
    {
        return exchange(m_pointer, nullptr);
    }

    NonNullReferencePointer<T> release_nonnull()
    {
        auto* ptr = leak_ref();
        VERIFY(ptr);
        return NonNullReferencePointer<T>(NonNullReferencePointer<T>::Adopt, *ptr);
    }

    ALWAYS_INLINE T* pointer() { return asPointer(); }
    ALWAYS_INLINE const T* pointer() const { return asPointer(); }

    ALWAYS_INLINE T* operator->() {

        return asNonNullPointer();
    }

    ALWAYS_INLINE const T* operator->() const {

        return asNonNullPointer();
    }

    ALWAYS_INLINE T& operator*() {

        return *asNonNullPointer();
    }

    ALWAYS_INLINE const T& operator*() const {

        return *asNonNullPointer();
    }

    ALWAYS_INLINE operator const T*() const { return asPointer(); }
    ALWAYS_INLINE operator T*() { return asPointer(); }

    ALWAYS_INLINE operator bool() { return !isNull(); }

    bool operator==(std::nullptr_t) const { return isNull(); }
    bool operator!=(std::nullptr_t) const { return !isNull(); }

    bool operator==(RefPtr const& other) const { return asPointer() == other.asPointer(); }
    bool operator!=(RefPtr const& other) const { return asPointer() != other.asPointer(); }

    bool operator==(RefPtr& other) { return asPointer() == other.asPointer(); }
    bool operator!=(RefPtr& other) { return asPointer() != other.asPointer(); }

    bool operator==(const T* other) const { return asPointer() == other; }
    bool operator!=(const T* other) const { return asPointer() != other; }

    bool operator==(T* other) { return asPointer() == other; }
    bool operator!=(T* other) { return asPointer() != other; }

    ALWAYS_INLINE bool isNull() const { return !m_pointer; }

private:

    ALWAYS_INLINE T* asPointer() const {

        return m_pointer;
    }

    ALWAYS_INLINE T* asNonNullPointer() const {

        VERIFY(m_pointer);
        
        return m_pointer;
    }

    T* m_pointer { nullptr };
};

template<typename T>
struct Formatter<RefPtr<T>> : Formatter<const T*> {

    ErrorOr<void> format(FormatBuilder& builder, RefPtr<T> const& value) {

        return Formatter<const T*>::format(builder, value.pointer());
    }
};

template<typename T>
struct Traits<RefPtr<T>> : public GenericTraits<RefPtr<T>> {
    
    using PeekType = T*;
    
    using ConstPeekType = const T*;
    
    static unsigned hash(RefPtr<T> const& p) { return pointerHash(p.pointer()); }
    
    static bool equals(RefPtr<T> const& a, RefPtr<T> const& b) { return a.pointer() == b.pointer(); }
};

template<typename T, typename U>
inline NonNullReferencePointer<T> static_ptr_cast(NonNullReferencePointer<U> const& ptr)
{
    return NonNullReferencePointer<T>(static_cast<const T&>(*ptr));
}

template<typename T, typename U>
inline RefPtr<T> static_ptr_cast(RefPtr<U> const& ptr) {

    return RefPtr<T>(static_cast<const T*>(ptr.pointer()));
}

template<typename T, typename U>
inline void swap(RefPtr<T>& a, RefPtr<U>& b) requires(IsConvertible<U*, T*>) {

    a.swap(b);
}

template<typename T>
inline RefPtr<T> adopt_ref_if_nonnull(T* object) {

    if (object) {

        return RefPtr<T>(RefPtr<T>::Adopt, *object);
    }

    return { };
}

template<typename T, class... Args>
requires(IsConstructible<T, Args...>) inline ErrorOr<NonNullReferencePointer<T>> try_make_ref_counted(Args&&... args) {

    return adoptNonNullReferenceOrErrorNoMemory(new (nothrow) T(forward<Args>(args)...));
}

// FIXME: Remove once P0960R3 is available in Clang.
template<typename T, class... Args>
inline ErrorOr<NonNullReferencePointer<T>> try_make_ref_counted(Args&&... args) {

    return adoptNonNullReferenceOrErrorNoMemory(new (nothrow) T { forward<Args>(args)... });
}

template<typename T>
inline ErrorOr<NonNullReferencePointer<T>> adoptNonNullReferenceOrErrorNoMemory(T* object) {

    auto result = adopt_ref_if_nonnull(object);

    if (!result) {

        return Error::fromErrorCode(ENOMEM);
    }

    return result.release_nonnull();
}

#endif
