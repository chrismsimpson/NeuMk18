/*
 * Copyright (c) 2022, Mustafa Quraish <mustafa@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashTable.h>
#include <initializer_list>

namespace NeuInternal {

template<typename T>
struct SetStorage : public ReferenceCounted<SetStorage<T>> {
    HashTable<T> table;
};

template<typename T>
class SetIterator {
    using Storage = SetStorage<T>;
    using Iterator = typename HashTable<T>::Iterator;

public:
    SetIterator(NonNullReferencePointer<Storage> storage)
        : m_storage(move(storage))
        , m_iterator(m_storage->table.begin())
    {
    }

    Optional<T> next()
    {
        if (m_iterator == m_storage->table.end())
            return {};
        auto res = *m_iterator;
        ++m_iterator;
        return res;
    }

private:
    NonNullReferencePointer<Storage> m_storage;
    Iterator m_iterator;
};

template<typename T>
class Set : public HashTable<T> {
private:
    using Storage = SetStorage<T>;

public:
    bool remove(T const& value) { return m_storage->table.remove(value); }
    bool contains(T const& value) const { return m_storage->table.contains(value); }

    ErrorOr<HashSetResult> add(T const& value) { return m_storage->table.set(value); }
    ErrorOr<HashSetResult> add(T&& value) { return m_storage->table.set(move(value)); }
    ErrorOr<void> ensureCapacity(size_t capacity) { return m_storage->table.tryEnsureCapacity(capacity); }

    bool isEmpty() const { return m_storage->table.isEmpty(); }
    size_t capacity() const { return m_storage->table.capacity(); }
    size_t size() const { return m_storage->table.size(); }
    void clear() { m_storage->table.clear(); }

    [[nodiscard]] UInt32 hash() const
    {
        UInt32 hash = 0;
        for (auto& value : *this) {
            hash = pairUInt32Hash(hash, value.hash());
        }
        return hash;
    }

    static ErrorOr<Set> create_empty()
    {
        auto storage = TRY(adoptNonNullReferenceOrErrorNoMemory(new (nothrow) Storage));
        return Set { move(storage) };
    }

    static ErrorOr<Set> create_with_values(std::initializer_list<T> list)
    {
        auto set = TRY(create_empty());
        TRY(set.ensureCapacity(list.size()));
        for (auto& value : list)
            TRY(set.add(value));
        return set;
    }

    SetIterator<T> iterator() const { return SetIterator<T> { m_storage }; }

private:
    explicit Set(NonNullReferencePointer<Storage> storage)
        : m_storage(move(storage))
    {
    }

    NonNullReferencePointer<Storage> m_storage;
};

}

using NeuInternal::Set;
