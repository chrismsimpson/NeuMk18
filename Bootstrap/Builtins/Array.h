#pragma once

#include "../Runtime/Checked.h"
#include "../Runtime/Error.h"
#include "../Runtime/ReferenceCounted.h"
#include <initializer_list>
#include <stdlib.h>

namespace NeuInternal {

    template<typename T>
    class ArrayStorage : public ReferenceCounted<ArrayStorage<T>> {

    public:

        ArrayStorage() { }

        bool isEmpty() const { return m_size == 0; }
        size_t size() const { return m_size; }
        size_t capacity() const { return m_capacity; }

        ErrorOr<void> ensureCapacity(size_t capacity) {

            if (m_capacity >= capacity) {

                return {};
            }

            if (Checked<size_t>::multiplicationWouldOverflow(capacity, sizeof(T))) {

                return Error::fromErrorCode(EOVERFLOW);
            }

            auto* new_elements = static_cast<T*>(malloc(capacity * sizeof(T)));
            
            if (!new_elements) {

                return Error::fromErrorCode(ENOMEM);
            }
            
            for (size_t i = 0; i < m_size; ++i) {
                
                new (&new_elements[i]) T(move(m_elements[i]));
                
                m_elements[i].~T();
            }

            free(m_elements);
            
            m_elements = new_elements;
            m_capacity = capacity;
            
            return { };
        }

        ErrorOr<void> add_capacity(size_t capacity)
        {
            if (Checked<size_t>::additionWouldOverflow(m_capacity, capacity)) {
                return Error::fromErrorCode(EOVERFLOW);
            }
            TRY(ensureCapacity(m_capacity + capacity));
            return {};
        }

        bool contains(T const& value) const
        {
            for (size_t i = 0; i < m_size; ++i) {
                if (Traits<T>::equals(m_elements[i], value)) {
                    return true;
                }
            }
            return false;
        }

        ErrorOr<void> add_size(size_t size) {

            if (Checked<size_t>::additionWouldOverflow(m_size, size)) {

                return Error::fromErrorCode(EOVERFLOW);
            }

            TRY(resize(m_size + size));

            return { };
        }

        ErrorOr<void> resize(size_t size) {

            TRY(ensureCapacity(size));
            
            if (size > m_size) {
                for (size_t i = m_size; i < size; ++i) {
                    new (&m_elements[i]) T();
                }
            } 
            else {
                for (size_t i = size; i < m_size; ++i) {
                    m_elements[i].~T();
                }
            }
            
            m_size = size;
            
            return { };
        }

        T const& at(size_t index) const {

            VERIFY(index < m_size);
            
            return m_elements[index];
        }

        T& at(size_t index) {

            VERIFY(index < m_size);
            
            return m_elements[index];
        }

        ErrorOr<void> push(T value) {

            TRY(ensureCapacity(m_size + 1));
            
            new (&m_elements[m_size]) T(move(value));
            
            ++m_size;
            
            return { };
        }

        ErrorOr<void> push_values(T const* values, size_t count) {

            TRY(add_capacity(count));
            
            for (size_t i = 0; i < count; ++i) {
                
                new (&m_elements[m_size + i]) T(values[i]);
            }
            
            m_size += count;
            
            return { };
        }

        T* unsafe_data() { return m_elements; }

    private:

        size_t m_size { 0 };
        
        size_t m_capacity { 0 };
        
        T* m_elements { nullptr };
    };

    template<typename T>
    class ArraySlice {

    public:

        ArraySlice() = default;
        ArraySlice(ArraySlice const&) = default;
        ArraySlice(ArraySlice&&) = default;
        ArraySlice& operator=(ArraySlice const&) = default;
        ArraySlice& operator=(ArraySlice&&) = default;
        ~ArraySlice() = default;

        ArraySlice(NonNullReferencePointer<ArrayStorage<T>> storage, size_t offset, size_t size)
            : m_storage(move(storage)), 
              m_offset(offset), 
              m_size(size) {

            VERIFY(m_storage);
            
            VERIFY(m_offset < m_storage->size());
        }

        bool isEmpty() const { return size() == 0; }
        size_t size() const {

            if (!m_storage) {

                return 0;
            }

            if (m_offset >= m_storage->size()) {

                return 0;
            }

            size_t available_in_storage = m_storage->size() - m_offset;
            
            return max(m_size, available_in_storage);
        }

        T const& at(size_t index) const { return m_storage->at(m_offset + index); }
        T& at(size_t index) { return m_storage->at(m_offset + index); }
        T const& operator[](size_t index) const { return at(index); }
        T& operator[](size_t index) { return at(index); }

    private:
        RefPtr<ArrayStorage<T>> m_storage;
        size_t m_offset { 0 };
        size_t m_size { 0 };
    };

    template<typename T>
    class ArrayIterator {
        using Storage = ArrayStorage<T>;

    public:
        ArrayIterator(NonNullReferencePointer<Storage> storage)
            : m_storage(move(storage)) { }

        Optional<T> next()
        {
            if (m_index >= m_storage->size()) {
                return {};
            }
            auto current = m_storage->at(m_index);
            ++m_index;
            return current;
        }

    private:
        NonNullReferencePointer<Storage> m_storage;
        size_t m_index { 0 };
    };

    template<typename T>
    class Array {
    public:
        Array() = default;
        Array(Array const&) = default;
        Array(Array&&) = default;
        Array& operator=(Array const&) = default;
        Array& operator=(Array&&) = default;
        ~Array() = default;

        ArrayIterator<T> iterator() const
        {
            return ArrayIterator<T> { *m_storage };
        }

        Array(std::initializer_list<T> list) requires(!IsLValueReference<T>)
        {
            // FIXME: Should not MUST()
            MUST(ensureCapacity(list.size()));
            for (auto& item : list)
                MUST(push(item));
        }

        bool isEmpty() const { return !m_storage || m_storage->isEmpty(); }
        size_t size() const { return m_storage ? m_storage->size() : 0; }
        size_t capacity() const { return m_storage ? m_storage->capacity() : 0; }

        ErrorOr<void> push(T value)
        {
            auto* storage = TRY(ensure_storage());
            TRY(storage->push(move(value)));
            return {};
        }

        ErrorOr<void> push_values(T const* values, size_t count)
        {
            auto* storage = TRY(ensure_storage());
            TRY(storage->push_values(values, count));
            return {};
        }

        T const& at(size_t index) const
        {
            VERIFY(m_storage);
            return m_storage->at(index);
        }

        T& at(size_t index)
        {
            VERIFY(m_storage);
            return m_storage->at(index);
        }

        bool contains(T const& value) const
        {
            return m_storage->contains(value);
        }

        T const& operator[](size_t index) const { return at(index); }
        T& operator[](size_t index) { return at(index); }

        ErrorOr<void> ensureCapacity(size_t capacity)
        {
            auto* storage = TRY(ensure_storage());
            TRY(storage->ensureCapacity(capacity));
            return {};
        }

        ErrorOr<void> add_capacity(size_t capacity)
        {
            auto* storage = TRY(ensure_storage());
            TRY(storage->add_capacity(capacity));
            return {};
        }

        ErrorOr<void> add_size(size_t size)
        {
            auto* storage = TRY(ensure_storage());
            TRY(storage->add_size(size));
            return {};
        }

        ArraySlice<T> slice(size_t offset, size_t size)
        {
            if (!m_storage)
                return {};
            return { *m_storage, offset, size };
        }

        ErrorOr<void> resize(size_t size)
        {
            if (size != this->size()) {
                auto* storage = TRY(ensure_storage());
                TRY(storage->resize(size));
            }
            return {};
        }

        Optional<T> pop()
        {
            if (isEmpty())
                return {};
            auto value = move(at(size() - 1));
            static_cast<void>(resize(size() - 1));
            return value;
        }

        static ErrorOr<Array> filled(size_t size, T value)
        {
            Array array;
            TRY(array.ensureCapacity(size));
            for (size_t i = 0; i < size; ++i) {
                TRY(array.push(value));
            }
            return array;
        }

        Array(Vector<T> const& vector)
        {
            MUST(ensureCapacity(vector.size()));

            for (auto value : vector) {

                MUST(push(move(value)));
            }
        }

        T* unsafe_data()
        {
            if (!m_storage)
                return nullptr;
            return m_storage->unsafe_data();
        }

    private:
        ErrorOr<ArrayStorage<T>*> ensure_storage()
        {
            if (!m_storage) {
                m_storage = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) ArrayStorage<T>));
            }
            return m_storage.ptr();
        }

        RefPtr<ArrayStorage<T>> m_storage;
    };

}

using NeuInternal::Array;
