/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "ReferenceCounted.h"
#include "ReferencePointer.h"
#include "Span.h"
#include "Types.h"
#include "kmalloc.h"

enum ShouldChomp {
    NoChomp,
    Chomp
};

size_t allocationSizeForStringImpl(size_t length);

class StringImpl : public ReferenceCounted<StringImpl> {
public:

    static NonNullReferencePointer<StringImpl> createUninitialized(size_t length, char*& buffer);
    static ReferencePointer<StringImpl> create(char const* cstring, ShouldChomp = NoChomp);
    static ReferencePointer<StringImpl> create(char const* cstring, size_t length, ShouldChomp = NoChomp);
    static ReferencePointer<StringImpl> create(ReadOnlyBytes, ShouldChomp = NoChomp);
    static ReferencePointer<StringImpl> create_lowercased(char const* cstring, size_t length);
    static ReferencePointer<StringImpl> create_uppercased(char const* cstring, size_t length);

    NonNullReferencePointer<StringImpl> to_lowercase() const;
    NonNullReferencePointer<StringImpl> to_uppercase() const;

    void operator delete(void* ptr) {

        kfree_sized(ptr, allocationSizeForStringImpl(static_cast<StringImpl*>(ptr)->m_length));
    }

    static StringImpl& theEmptyStringImpl();

    ~StringImpl();

    size_t length() const { return m_length; }
    // Includes NUL-terminator.
    char const* characters() const { return &m_inline_buffer[0]; }

    ALWAYS_INLINE ReadOnlyBytes bytes() const { return { characters(), length() }; }
    ALWAYS_INLINE StringView view() const { return { characters(), length() }; }

    char const& operator[](size_t i) const
    {
        VERIFY(i < m_length);
        return characters()[i];
    }

    bool operator==(StringImpl const& other) const
    {
        if (length() != other.length())
            return false;
        return __builtin_memcmp(characters(), other.characters(), length()) == 0;
    }

    unsigned hash() const
    {
        if (!m_has_hash)
            compute_hash();
        return m_hash;
    }

    unsigned existing_hash() const
    {
        return m_hash;
    }

    unsigned caseInsensitiveHash() const;

private:

    enum ConstructTheEmptyStringImplTag {

        ConstructTheEmptyStringImpl
    };

    explicit StringImpl(ConstructTheEmptyStringImplTag) {

        m_inline_buffer[0] = '\0';
    }

    enum ConstructWithInlineBufferTag {

        ConstructWithInlineBuffer
    };

    StringImpl(ConstructWithInlineBufferTag, size_t length);

    void compute_hash() const;

    size_t m_length { 0 };
    
    mutable unsigned m_hash { 0 };
    
    mutable bool m_has_hash { false };
    
    char m_inline_buffer[0];
};

inline size_t allocationSizeForStringImpl(size_t length) {

    return sizeof(StringImpl) + (sizeof(char) * length) + sizeof(char);
}

template<>
struct Formatter<StringImpl> : Formatter<StringView> {

    ErrorOr<void> format(FormatBuilder& builder, StringImpl const& value) {

        return Formatter<StringView>::format(builder, { value.characters(), value.length() });
    }
};
