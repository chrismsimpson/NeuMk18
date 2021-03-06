/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "../Runtime/HashMap.h"
#include "../Runtime/NonNullReferencePointer.h"
#include "../Runtime/ReferenceCounted.h"
#include "../Runtime/Tuple.h"

namespace NeuInternal {

template<typename K, typename V>
struct DictionaryStorage : public ReferenceCounted<DictionaryStorage<K, V>> {
    HashMap<K, V> map;
};

template<typename K, typename V>
class DictionaryIterator {
    using Storage = DictionaryStorage<K, V>;
    using Iterator = typename HashMap<K, V>::IteratorType;

public:
    DictionaryIterator(NonNullReferencePointer<Storage> storage)
        : m_storage(move(storage))
        , m_iterator(m_storage->map.begin())
    {
    }

    Optional<Tuple<K, V>> next()
    {
        if (m_iterator == m_storage->map.end())
            return {};
        auto res = *m_iterator;
        ++m_iterator;
        return Tuple<K, V>(res.key, res.value);
    }

private:
    NonNullReferencePointer<Storage> m_storage;
    Iterator m_iterator;
};

template<typename K, typename V>
class Dictionary {
    using Storage = DictionaryStorage<K, V>;

public:
    bool isEmpty() const { return m_storage->map.isEmpty(); }
    size_t size() const { return m_storage->map.size(); }
    void clear() { m_storage->map.clear(); }

    ErrorOr<void> set(K const& key, V value)
    {
        TRY(m_storage->map.set(key, move(value)));
        return {};
    }

    bool remove(K const& key) { return m_storage->map.remove(key); }
    bool contains(K const& key) const { return m_storage->map.contains(key); }

    Optional<V> get(K const& key) const { return m_storage->map.get(key); }
    V& operator[](K const& key) { return m_storage->map.get(key).value(); }
    V const& operator[](K const& key) const { return m_storage->map.get(key).value(); }

    Vector<K> keys() const { return m_storage->map.keys(); }

    ErrorOr<void> ensureCapacity(size_t capacity)
    {
        TRY(m_storage->map.ensureCapacity(capacity));
        return {};
    }

    // FIXME: Remove this constructor once Jakt knows how to call Dictionary::create_empty()
    Dictionary()
        : m_storage(MUST(adoptNonNullReferenceOrErrorNoMemory(new (nothrow) Storage)))
    {
    }

    static ErrorOr<Dictionary> create_empty()
    {
        auto storage = TRY(adoptNonNullReferenceOrErrorNoMemory(new (nothrow) Storage));
        return Dictionary { move(storage) };
    }

    struct Entry {
        K key;
        V value;
    };
    static ErrorOr<Dictionary> create_with_entries(std::initializer_list<Entry> list)
    {
        auto dictionary = TRY(create_empty());
        TRY(dictionary.ensureCapacity(list.size()));
        for (auto& item : list)
            TRY(dictionary.set(item.key, item.value));
        return dictionary;
    }

    DictionaryIterator<K, V> iterator() const { return DictionaryIterator<K, V> { m_storage }; }

private:
    explicit Dictionary(NonNullReferencePointer<Storage> storage)
        : m_storage(move(storage))
    {
    }

    NonNullReferencePointer<Storage> m_storage;
};

}

using NeuInternal::Dictionary;
