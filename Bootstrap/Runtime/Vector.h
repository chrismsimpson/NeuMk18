/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Assertions.h"
#include "Error.h"
#include "Find.h"
#include "Forward.h"
#include "Iterator.h"
#include "Optional.h"
#include "ReverseIterator.h"
#include "Span.h"
#include "StdLibExtras.h"
#include "Traits.h"
#include "TypedTransfer.h"
#include "kmalloc.h"
#include <initializer_list>

namespace Detail {

    template<typename StorageType, bool>
    struct CanBePlacedInsideVectorHelper;

    template<typename StorageType>
    struct CanBePlacedInsideVectorHelper<StorageType, true> {

        template<typename U>
        static constexpr bool value = requires(U&& u) { StorageType { &u }; };
    };

    template<typename StorageType>
    struct CanBePlacedInsideVectorHelper<StorageType, false> {

        template<typename U>
        static constexpr bool value = requires(U&& u) { StorageType(forward<U>(u)); };
    };
}

template<typename T, size_t InlineCapacity>
requires(!IsRValueReference<T>) class Vector {

private:

    static constexpr bool ContainsReference = IsLValueReference<T>;

    using StorageType = Conditional<ContainsReference, RawPtr<RemoveReference<T>>, T>;

    using VisibleType = RemoveReference<T>;

    template<typename U>
    static constexpr bool CanBePlacedInsideVector = Detail::CanBePlacedInsideVectorHelper<StorageType, ContainsReference>::template value<U>;

public:

    using ValueType = T;

    Vector()
        : m_capacity(InlineCapacity) { }

    Vector(std::initializer_list<T> list) requires(!IsLValueReference<T>) {

        ensureCapacity(list.size());

        for (auto& item : list) {

            uncheckedAppend(item);
        }
    }

    Vector(Vector&& other)
        : m_size(other.m_size), 
          m_capacity(other.m_capacity), 
          m_outline_buffer(other.m_outline_buffer) {

        if constexpr (InlineCapacity > 0) {

            if (!m_outline_buffer) {

                for (size_t i = 0; i < m_size; ++i) {

                    new (&inline_buffer()[i]) StorageType(move(other.inline_buffer()[i]));
                    
                    other.inline_buffer()[i].~StorageType();
                }
            }
        }

        other.m_outline_buffer = nullptr;
        other.m_size = 0;
        other.resetCapacity();
    }

    Vector(Vector const& other) {

        ensureCapacity(other.size());
        
        TypedTransfer<StorageType>::copy(data(), other.data(), other.size());
        
        m_size = other.size();
    }

    explicit Vector(Span<T const> other) requires(!IsLValueReference<T>) {

        ensureCapacity(other.size());
        
        TypedTransfer<StorageType>::copy(data(), other.data(), other.size());
        
        m_size = other.size();
    }

    template<size_t otherInlineCapacity>
    Vector(Vector<T, otherInlineCapacity> const& other) {

        ensureCapacity(other.size());
        
        TypedTransfer<StorageType>::copy(data(), other.data(), other.size());
        
        m_size = other.size();
    }

    ~Vector() {

        clear();
    }

    Span<StorageType> span() { return { data(), size() }; }
    Span<StorageType const> span() const { return { data(), size() }; }

    operator Span<StorageType>() { return span(); }
    operator Span<StorageType const>() const { return span(); }

    bool isEmpty() const { return size() == 0; }
    ALWAYS_INLINE size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }

    ALWAYS_INLINE StorageType* data() {

        if constexpr (InlineCapacity > 0) {

            return m_outline_buffer ? m_outline_buffer : inline_buffer();
        }

        return m_outline_buffer;
    }

    ALWAYS_INLINE StorageType const* data() const {

        if constexpr (InlineCapacity > 0) {

            return m_outline_buffer ? m_outline_buffer : inline_buffer();
        }

        return m_outline_buffer;
    }

    ALWAYS_INLINE VisibleType const& at(size_t i) const {

        VERIFY(i < m_size);
        
        if constexpr (ContainsReference) {

            return *data()[i];
        }
        else {

            return data()[i];
        }
    }

    ALWAYS_INLINE VisibleType& at(size_t i) {

        VERIFY(i < m_size);

        if constexpr (ContainsReference) {

            return *data()[i];
        }
        else {

            return data()[i];
        }
    }

    ALWAYS_INLINE VisibleType const& operator[](size_t i) const { return at(i); }
    ALWAYS_INLINE VisibleType& operator[](size_t i) { return at(i); }

    VisibleType const& first() const { return at(0); }
    VisibleType& first() { return at(0); }

    VisibleType const& last() const { return at(size() - 1); }
    VisibleType& last() { return at(size() - 1); }

    template<typename TUnaryPredicate>
    Optional<VisibleType&> firstMatching(TUnaryPredicate predicate) requires(!ContainsReference) {

        for (size_t i = 0; i < size(); ++i) {

            if (predicate(at(i))) {

                return at(i);
            }
        }

        return { };
    }

    template<typename TUnaryPredicate>
    Optional<VisibleType const&> firstMatching(TUnaryPredicate predicate) const requires(!ContainsReference) {

        for (size_t i = 0; i < size(); ++i) {

            if (predicate(at(i))) {

                return Optional<VisibleType const&>(at(i));
            }
        }

        return { };
    }

    template<typename TUnaryPredicate>
    Optional<VisibleType&> last_matching(TUnaryPredicate predicate) requires(!ContainsReference) {

        for (ssize_t i = size() - 1; i >= 0; --i) {

            if (predicate(at(i))) {

                return at(i);
            }
        }

        return { };
    }

    template<typename V>
    bool operator==(V const& other) const {

        if (m_size != other.size()) {

            return false;
        }

        return TypedTransfer<StorageType>::compare(data(), other.data(), size());
    }

    template<typename V>
    bool contains_slow(V const& value) const {

        for (size_t i = 0; i < size(); ++i) {

            if (Traits<VisibleType>::equals(at(i), value)) {

                return true;
            }
        }

        return false;
    }

    bool contains_in_range(VisibleType const& value, size_t const start, size_t const end) const {

        VERIFY(start <= end);
        VERIFY(end < size());

        for (size_t i = start; i <= end; ++i) {

            if (Traits<VisibleType>::equals(at(i), value)) {

                return true;
            }
        }

        return false;
    }

#ifndef KERNEL

    template<typename U = T>
    void insert(size_t index, U&& value) requires(CanBePlacedInsideVector<U>)
    {
        MUST(try_insert<U>(index, forward<U>(value)));
    }

    template<typename TUnaryPredicate, typename U = T>
    void insert_before_matching(U&& value, TUnaryPredicate predicate, size_t first_index = 0, size_t* inserted_index = nullptr) requires(CanBePlacedInsideVector<U>)
    {
        MUST(try_insert_before_matching(forward<U>(value), predicate, first_index, inserted_index));
    }

    void extend(Vector&& other)
    {
        MUST(try_extend(move(other)));
    }

    void extend(Vector const& other)
    {
        MUST(try_extend(other));
    }

#endif

    ALWAYS_INLINE void append(T&& value) {

        if constexpr (ContainsReference) {
            
            MUST(tryAppend(value));
        }
        else {

            MUST(tryAppend(move(value)));
        }
    }

    ALWAYS_INLINE void append(T const& value) requires(!ContainsReference) {

        MUST(tryAppend(T(value)));
    }

#ifndef KERNEL

    void append(StorageType const* values, size_t count) {

        MUST(tryAppend(values, count));
    }

#endif

    template<typename U = T>
    ALWAYS_INLINE void uncheckedAppend(U&& value) requires(CanBePlacedInsideVector<U>) {

        VERIFY((size() + 1) <= capacity());

        if constexpr (ContainsReference) {

            new (slot(m_size)) StorageType(&value);
        }
        else {

            new (slot(m_size)) StorageType(forward<U>(value));
        }

        ++m_size;
    }

    ALWAYS_INLINE void uncheckedAppend(StorageType const* values, size_t count) {

        if (count == 0) {

            return;
        }

        VERIFY((size() + count) <= capacity());
        
        TypedTransfer<StorageType>::copy(slot(m_size), values, count);
        
        m_size += count;
    }

#ifndef KERNEL

    template<class... Args>
    void empend(Args&&... args) requires(!ContainsReference) {

        MUST(try_empend(forward<Args>(args)...));
    }

    template<typename U = T>
    void prepend(U&& value) requires(CanBePlacedInsideVector<U>) {

        MUST(try_insert(0, forward<U>(value)));
    }

    void prepend(Vector&& other) {

        MUST(try_prepend(move(other)));
    }

    void prepend(StorageType const* values, size_t count) {

        MUST(try_prepend(values, count));
    }

#endif

    // FIXME: What about assigning from a vector with lower inline capacity?
    
    Vector& operator=(Vector&& other) {

        if (this != &other) {

            clear();
            
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_outline_buffer = other.m_outline_buffer;
            
            if constexpr (InlineCapacity > 0) {

                if (!m_outline_buffer) {
                    
                    for (size_t i = 0; i < m_size; ++i) {

                        new (&inline_buffer()[i]) StorageType(move(other.inline_buffer()[i]));
                        
                        other.inline_buffer()[i].~StorageType();
                    }
                }
            }

            other.m_outline_buffer = nullptr;
            other.m_size = 0;
            other.resetCapacity();
        }

        return *this;
    }

    Vector& operator=(Vector const& other) {

        if (this != &other) {

            clear();
            
            ensureCapacity(other.size());
            
            TypedTransfer<StorageType>::copy(data(), other.data(), other.size());
            
            m_size = other.size();
        }

        return *this;
    }

    template<size_t otherInlineCapacity>
    Vector& operator=(Vector<T, otherInlineCapacity> const& other) {

        clear();

        ensureCapacity(other.size());

        TypedTransfer<StorageType>::copy(data(), other.data(), other.size());
        
        m_size = other.size();
        
        return *this;
    }

    void clear() {

        clearWithCapacity();

        if (m_outline_buffer) {

            kfree_sized(m_outline_buffer, m_capacity * sizeof(StorageType));
            
            m_outline_buffer = nullptr;
        }

        resetCapacity();
    }

    void clearWithCapacity() {

        for (size_t i = 0; i < m_size; ++i) {

            data()[i].~StorageType();
        }

        m_size = 0;
    }

    void remove(size_t index) {

        VERIFY(index < m_size);

        if constexpr (Traits<StorageType>::isTrivial()) {
            
            TypedTransfer<StorageType>::copy(slot(index), slot(index + 1), m_size - index - 1);
        } 
        else {

            at(index).~StorageType();

            for (size_t i = index + 1; i < m_size; ++i) {

                new (slot(i - 1)) StorageType(move(at(i)));
                
                at(i).~StorageType();
            }
        }

        --m_size;
    }

    void remove(size_t index, size_t count) {

        if (count == 0) {

            return;
        }
        
        VERIFY(index + count > index);
        VERIFY(index + count <= m_size);

        if constexpr (Traits<StorageType>::isTrivial()) {

            TypedTransfer<StorageType>::copy(slot(index), slot(index + count), m_size - index - count);
        } 
        else {

            for (size_t i = index; i < index + count; i++) {

                at(i).~StorageType();
            }

            for (size_t i = index + count; i < m_size; ++i) {

                new (slot(i - count)) StorageType(move(at(i)));
                
                at(i).~StorageType();
            }
        }

        m_size -= count;
    }

    template<typename TUnaryPredicate>
    bool removeFirstMatching(TUnaryPredicate predicate) {

        for (size_t i = 0; i < size(); ++i) {

            if (predicate(at(i))) {

                remove(i);

                return true;
            }
        }
        
        return false;
    }

    template<typename TUnaryPredicate>
    bool removeAllMatching(TUnaryPredicate predicate) {

        bool somethingWasRemoved = false;

        for (size_t i = 0; i < size();) {

            if (predicate(at(i))) {
                
                remove(i);
                
                somethingWasRemoved = true;
            } 
            else {

                ++i;
            }
        }

        return somethingWasRemoved;
    }

    ALWAYS_INLINE T takeLast() {

        VERIFY(!isEmpty());
        
        auto value = move(rawLast());
        
        if constexpr (!ContainsReference) {

            last().~T();
        }

        --m_size;
        
        if constexpr (ContainsReference) {

            return *value;
        }
        else {

            return value;
        }
    }

    T take_first() {

        VERIFY(!isEmpty());
        
        auto value = move(rawFirst());
        
        remove(0);
        
        if constexpr (ContainsReference) {

            return *value;
        }
        else {

            return value;
        }
    }

    T take(size_t index) {

        auto value = move(rawAt(index));
        
        remove(index);
        
        if constexpr (ContainsReference) {

            return *value;
        }
        else {

            return value;
        }
    }

    T unstableTake(size_t index) {

        VERIFY(index < m_size);
        
        swap(rawAt(index), rawAt(m_size - 1));
        
        return takeLast();
    }

    template<typename U = T>
    ErrorOr<void> try_insert(size_t index, U&& value) requires(CanBePlacedInsideVector<U>) {

        if (index > size()) {

            return Error::fromErrorCode(EINVAL);
        }

        if (index == size()) {

            return tryAppend(forward<U>(value));
        }

        TRY(try_grow_capacity(size() + 1));
        
        ++m_size;
        
        if constexpr (Traits<StorageType>::isTrivial()) {
            
            TypedTransfer<StorageType>::move(slot(index + 1), slot(index), m_size - index - 1);
        } 
        else {

            for (size_t i = size() - 1; i > index; --i) {

                new (slot(i)) StorageType(move(at(i - 1)));

                at(i - 1).~StorageType();
            }
        }

        if constexpr (ContainsReference) {

            new (slot(index)) StorageType(&value);
        }
        else {

            new (slot(index)) StorageType(forward<U>(value));
        }

        return { };
    }

    template<typename TUnaryPredicate, typename U = T>
    ErrorOr<void> try_insert_before_matching(U&& value, TUnaryPredicate predicate, size_t first_index = 0, size_t* inserted_index = nullptr) requires(CanBePlacedInsideVector<U>) {

        for (size_t i = first_index; i < size(); ++i) {

            if (predicate(at(i))) {

                TRY(try_insert(i, forward<U>(value)));
                
                if (inserted_index) {

                    *inserted_index = i;
                }

                return { };
            }
        }

        TRY(tryAppend(forward<U>(value)));

        if (inserted_index) {

            *inserted_index = size() - 1;
        }

        return { };
    }

    ErrorOr<void> try_extend(Vector&& other) {

        if (isEmpty() && capacity() <= other.capacity()) {

            *this = move(other);
            
            return { };
        }
        
        auto other_size = other.size();
        
        Vector tmp = move(other);
        
        TRY(try_grow_capacity(size() + other_size));
        
        TypedTransfer<StorageType>::move(data() + m_size, tmp.data(), other_size);
        
        m_size += other_size;
        
        return { };
    }

    ErrorOr<void> try_extend(Vector const& other) {

        TRY(try_grow_capacity(size() + other.size()));
        
        TypedTransfer<StorageType>::copy(data() + m_size, other.data(), other.size());
        
        m_size += other.m_size;
        
        return { };
    }

    ErrorOr<void> tryAppend(T&& value) {

        TRY(try_grow_capacity(size() + 1));
        
        if constexpr (ContainsReference) {

            new (slot(m_size)) StorageType(&value);
        }
        else {

            new (slot(m_size)) StorageType(move(value));
        }

        ++m_size;
        
        return { };
    }

    ErrorOr<void> tryAppend(T const& value) requires(!ContainsReference) {

        return tryAppend(T(value));
    }

    ErrorOr<void> tryAppend(StorageType const* values, size_t count) {

        if (count == 0) {

            return {};
        }

        TRY(try_grow_capacity(size() + count));
        
        TypedTransfer<StorageType>::copy(slot(m_size), values, count);
        
        m_size += count;
        
        return { };
    }

    template<class... Args>
    ErrorOr<void> try_empend(Args&&... args) requires(!ContainsReference) {

        TRY(try_grow_capacity(m_size + 1));
        
        new (slot(m_size)) StorageType { forward<Args>(args)... };
        
        ++m_size;
        
        return { };
    }

    template<typename U = T>
    ErrorOr<void> try_prepend(U&& value) requires(CanBePlacedInsideVector<U>) {

        return try_insert(0, forward<U>(value));
    }

    ErrorOr<void> try_prepend(Vector&& other) {

        if (other.isEmpty()) {

            return { };
        }

        if (isEmpty()) {

            *this = move(other);
            
            return { };
        }

        auto other_size = other.size();

        TRY(try_grow_capacity(size() + other_size));

        for (size_t i = size() + other_size - 1; i >= other.size(); --i) {

            new (slot(i)) StorageType(move(at(i - other_size)));

            at(i - other_size).~StorageType();
        }

        Vector tmp = move(other);
        
        TypedTransfer<StorageType>::move(slot(0), tmp.data(), tmp.size());
        
        m_size += other_size;
        
        return { };
    }

    ErrorOr<void> try_prepend(StorageType const* values, size_t count) {

        if (count == 0) {

            return { };
        }

        TRY(try_grow_capacity(size() + count));
        
        TypedTransfer<StorageType>::move(slot(count), slot(0), m_size);
        
        TypedTransfer<StorageType>::copy(slot(0), values, count);
        
        m_size += count;
        
        return { };
    }

    ErrorOr<void> try_grow_capacity(size_t needed_capacity) {

        if (m_capacity >= needed_capacity) {

            return { };
        }

        return tryEnsureCapacity(paddedCapacity(needed_capacity));
    }

    ErrorOr<void> tryEnsureCapacity(size_t needed_capacity) {

        if (m_capacity >= needed_capacity) {

            return { };
        }

        size_t new_capacity = kmalloc_good_size(needed_capacity * sizeof(StorageType)) / sizeof(StorageType);
        auto* new_buffer = static_cast<StorageType*>(kmalloc_array(new_capacity, sizeof(StorageType)));

        if (new_buffer == nullptr) {

            return Error::fromErrorCode(ENOMEM);
        }

        if constexpr (Traits<StorageType>::isTrivial()) {
            
            TypedTransfer<StorageType>::copy(new_buffer, data(), m_size);
        }
        else {

            for (size_t i = 0; i < m_size; ++i) {

                new (&new_buffer[i]) StorageType(move(at(i)));

                at(i).~StorageType();
            }
        }

        if (m_outline_buffer) {

            kfree_sized(m_outline_buffer, m_capacity * sizeof(StorageType));
        }

        m_outline_buffer = new_buffer;
        
        m_capacity = new_capacity;
        
        return { };
    }

    ErrorOr<void> try_resize(size_t new_size, bool keep_capacity = false) requires(!ContainsReference) {

        if (new_size <= size()) {

            shrink(new_size, keep_capacity);
            
            return { };
        }

        TRY(tryEnsureCapacity(new_size));

        for (size_t i = size(); i < new_size; ++i) {

            new (slot(i)) StorageType {};
        }

        m_size = new_size;
        
        return { };
    }

    ErrorOr<void> tryResizeAndKeepCapacity(size_t new_size) requires(!ContainsReference) {

        return try_resize(new_size, true);
    }

    void grow_capacity(size_t needed_capacity) {

        MUST(try_grow_capacity(needed_capacity));
    }

    void ensureCapacity(size_t needed_capacity) {

        MUST(tryEnsureCapacity(needed_capacity));
    }

    void shrink(size_t new_size, bool keep_capacity = false) {

        VERIFY(new_size <= size());

        if (new_size == size()) {

            return;
        }

        if (new_size == 0) {

            if (keep_capacity) {

                clearWithCapacity();
            }
            else {

                clear();
            }

            return;
        }

        for (size_t i = new_size; i < size(); ++i) {

            at(i).~StorageType();
        }

        m_size = new_size;
    }

    void resize(size_t new_size, bool keep_capacity = false) requires(!ContainsReference) {

        MUST(try_resize(new_size, keep_capacity));
    }

    void resize_and_keep_capacity(size_t new_size) requires(!ContainsReference) {

        MUST(tryResizeAndKeepCapacity(new_size));
    }

    using ConstIterator = SimpleIterator<Vector const, VisibleType const>;
    using Iterator = SimpleIterator<Vector, VisibleType>;
    using ReverseIterator = SimpleReverseIterator<Vector, VisibleType>;
    using ReverseConstIterator = SimpleReverseIterator<Vector const, VisibleType const>;

    ConstIterator begin() const { return ConstIterator::begin(*this); }
    Iterator begin() { return Iterator::begin(*this); }
    ReverseIterator rbegin() { return ReverseIterator::rbegin(*this); }
    ReverseConstIterator rbegin() const { return ReverseConstIterator::rbegin(*this); }

    ConstIterator end() const { return ConstIterator::end(*this); }
    Iterator end() { return Iterator::end(*this); }
    ReverseIterator rend() { return ReverseIterator::rend(*this); }
    ReverseConstIterator rend() const { return ReverseConstIterator::rend(*this); }

    ALWAYS_INLINE constexpr auto inReverse() {

        return ReverseWrapper::inReverse(*this);
    }

    ALWAYS_INLINE constexpr auto inReverse() const {

        return ReverseWrapper::inReverse(*this);
    }

    template<typename TUnaryPredicate>
    ConstIterator findIf(TUnaryPredicate&& finder) const {

        return findIf(begin(), end(), forward<TUnaryPredicate>(finder));
    }

    template<typename TUnaryPredicate>
    Iterator findIf(TUnaryPredicate&& finder) {

        return findIf(begin(), end(), forward<TUnaryPredicate>(finder));
    }

    ConstIterator find(VisibleType const& value) const {

        return find(begin(), end(), value);
    }

    Iterator find(VisibleType const& value) {

        return find(begin(), end(), value);
    }

    Optional<size_t> findFirstIndex(VisibleType const& value) const {
        
        if (auto const index = findIndex(begin(), end(), value); index < size()) {

            return index;
        }

        return { };
    }

    void reverse() {

        for (size_t i = 0; i < size() / 2; ++i) {

            ::swap(at(i), at(size() - i - 1));
        }
    }

private:

    void resetCapacity() {

        m_capacity = InlineCapacity;
    }

    static size_t paddedCapacity(size_t capacity) {

        return max(static_cast<size_t>(4), capacity + (capacity / 4) + 4);
    }

    StorageType* slot(size_t i) { return &data()[i]; }
    StorageType const* slot(size_t i) const { return &data()[i]; }

    StorageType* inline_buffer()
    {
        static_assert(InlineCapacity > 0);
        return reinterpret_cast<StorageType*>(m_inline_buffer_storage);
    }
    StorageType const* inline_buffer() const
    {
        static_assert(InlineCapacity > 0);
        return reinterpret_cast<StorageType const*>(m_inline_buffer_storage);
    }

    StorageType& rawLast() { return rawAt(size() - 1); }
    StorageType& rawFirst() { return rawAt(0); }
    StorageType& rawAt(size_t index) { return *slot(index); }

    size_t m_size { 0 };
    size_t m_capacity { 0 };

    static constexpr size_t storage_size() {

        if constexpr (InlineCapacity == 0) {

            return 0;
        }
        else {

            return sizeof(StorageType) * InlineCapacity;
        }
    }

    static constexpr size_t storage_alignment() {

        if constexpr (InlineCapacity == 0) {

            return 1;
        }
        else {

            return alignof(StorageType);
        }
    }

    alignas(storage_alignment()) unsigned char m_inline_buffer_storage[storage_size()];
    StorageType* m_outline_buffer { nullptr };
};

template<class... Args>
Vector(Args... args) -> Vector<CommonType<Args...>>;
