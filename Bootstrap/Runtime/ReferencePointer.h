/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define REFPTR_SCRUB_BYTE 0xe0

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeReferencePointer.h>
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
class [[nodiscard]] ReferencePointer {

    template<typename U>
    friend class ReferencePointer;

    template<typename U>
    friend class WeakPointer;

public:

    enum AdoptTag {
        Adopt
    };

    ReferencePointer() = default;

    ReferencePointer(T const* ptr)
        : m_pointer(const_cast<T*>(ptr)) {

        refIfNotNull(m_pointer);
    }

    ReferencePointer(T const& object)
        : m_pointer(const_cast<T*>(&object)) {

        m_pointer->ref();
    }

    ReferencePointer(AdoptTag, T& object)
        : m_pointer(&object) { }

    ReferencePointer(ReferencePointer&& other)
        : m_pointer(other.leak_ref()) { }

    ALWAYS_INLINE ReferencePointer(NonNullReferencePointer<T> const& other)
        : m_pointer(const_cast<T*>(other.pointer())) {

        m_pointer->ref();
    }

    template<typename U>
    ALWAYS_INLINE ReferencePointer(NonNullReferencePointer<U> const& other) requires(IsConvertible<U*, T*>)
        : m_pointer(const_cast<T*>(static_cast<T const*>(other.pointer()))) {

        m_pointer->ref();
    }

    template<typename U>
    ALWAYS_INLINE ReferencePointer(NonNullReferencePointer<U>&& other) requires(IsConvertible<U*, T*>)
        : m_pointer(static_cast<T*>(&other.leak_ref())) { }

    template<typename U>
    ReferencePointer(ReferencePointer<U>&& other) requires(IsConvertible<U*, T*>)
        : m_pointer(static_cast<T*>(other.leak_ref())) { }

    ReferencePointer(ReferencePointer const& other)
        : m_pointer(other.m_pointer) {

        refIfNotNull(m_pointer);
    }

    template<typename U>
    ReferencePointer(ReferencePointer<U> const& other) requires(IsConvertible<U*, T*>)
        : m_pointer(const_cast<T*>(static_cast<T const*>(other.pointer()))) {

        refIfNotNull(m_pointer);
    }

    ALWAYS_INLINE ~ReferencePointer()
    {
        clear();
#    ifdef SANITIZE_PTRS
        m_pointer = reinterpret_cast<T*>(explode_byte(REFPTR_SCRUB_BYTE));
#    endif
    }

    void swap(ReferencePointer& other)
    {
        ::swap(m_pointer, other.m_pointer);
    }

    template<typename U>
    void swap(ReferencePointer<U>& other) requires(IsConvertible<U*, T*>) {

        ::swap(m_pointer, other.m_pointer);
    }

    ALWAYS_INLINE ReferencePointer& operator=(ReferencePointer&& other) {

        ReferencePointer tmp { move(other) };
        
        swap(tmp);
        
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE ReferencePointer& operator=(ReferencePointer<U>&& other) requires(IsConvertible<U*, T*>) {

        ReferencePointer tmp { move(other) };
        swap(tmp);
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE ReferencePointer& operator=(NonNullReferencePointer<U>&& other) requires(IsConvertible<U*, T*>) {

        ReferencePointer tmp { move(other) };
        
        swap(tmp);
        
        return *this;
    }

    ALWAYS_INLINE ReferencePointer& operator=(NonNullReferencePointer<T> const& other) {

        ReferencePointer tmp { other };
        
        swap(tmp);
        
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE ReferencePointer& operator=(NonNullReferencePointer<U> const& other) requires(IsConvertible<U*, T*>) {

        ReferencePointer tmp { other };
        
        swap(tmp);
        
        return *this;
    }

    ALWAYS_INLINE ReferencePointer& operator=(ReferencePointer const& other) {

        ReferencePointer tmp { other };
        
        swap(tmp);
        
        return *this;
    }

    template<typename U>
    ALWAYS_INLINE ReferencePointer& operator=(ReferencePointer<U> const& other) requires(IsConvertible<U*, T*>)
    {
        ReferencePointer tmp { other };
        swap(tmp);
        return *this;
    }

    ALWAYS_INLINE ReferencePointer& operator=(T const* ptr) {

        ReferencePointer tmp { ptr };
        
        swap(tmp);
        
        return *this;
    }

    ALWAYS_INLINE ReferencePointer& operator=(T const& object) {

        ReferencePointer tmp { object };
        
        swap(tmp);
        
        return *this;
    }

    ReferencePointer& operator=(std::nullptr_t) {

        clear();
        
        return *this;
    }

    ALWAYS_INLINE bool assign_if_null(ReferencePointer&& other) {

        if (this == &other) {

            return isNull();
        }

        *this = move(other);
        
        return true;
    }

    template<typename U>
    ALWAYS_INLINE bool assign_if_null(ReferencePointer<U>&& other) {

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

    bool operator==(ReferencePointer const& other) const { return asPointer() == other.asPointer(); }
    bool operator!=(ReferencePointer const& other) const { return asPointer() != other.asPointer(); }

    bool operator==(ReferencePointer& other) { return asPointer() == other.asPointer(); }
    bool operator!=(ReferencePointer& other) { return asPointer() != other.asPointer(); }

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
struct Formatter<ReferencePointer<T>> : Formatter<const T*> {

    ErrorOr<void> format(FormatBuilder& builder, ReferencePointer<T> const& value) {

        return Formatter<const T*>::format(builder, value.pointer());
    }
};

template<typename T>
struct Traits<ReferencePointer<T>> : public GenericTraits<ReferencePointer<T>> {
    
    using PeekType = T*;
    
    using ConstPeekType = const T*;
    
    static unsigned hash(ReferencePointer<T> const& p) { return pointerHash(p.pointer()); }
    
    static bool equals(ReferencePointer<T> const& a, ReferencePointer<T> const& b) { return a.pointer() == b.pointer(); }
};

template<typename T, typename U>
inline NonNullReferencePointer<T> static_ptr_cast(NonNullReferencePointer<U> const& ptr)
{
    return NonNullReferencePointer<T>(static_cast<const T&>(*ptr));
}

template<typename T, typename U>
inline ReferencePointer<T> static_ptr_cast(ReferencePointer<U> const& ptr) {

    return ReferencePointer<T>(static_cast<const T*>(ptr.pointer()));
}

template<typename T, typename U>
inline void swap(ReferencePointer<T>& a, ReferencePointer<U>& b) requires(IsConvertible<U*, T*>) {

    a.swap(b);
}

template<typename T>
inline ReferencePointer<T> adopt_ref_if_nonnull(T* object) {

    if (object) {

        return ReferencePointer<T>(ReferencePointer<T>::Adopt, *object);
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
