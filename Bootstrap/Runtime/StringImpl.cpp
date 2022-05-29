/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "CharacterTypes.h"
#include "HashTable.h"
#include "Memory.h"
#include "StdLibExtras.h"
#include "StringHash.h"
#include "StringImpl.h"
#include "kmalloc.h"

static StringImpl* s_theEmptyStringImpl = nullptr;

StringImpl& StringImpl::theEmptyStringImpl() {

    if (!s_theEmptyStringImpl) {

        void* slot = kmalloc(sizeof(StringImpl) + sizeof(char));
        
        s_theEmptyStringImpl = new (slot) StringImpl(ConstructTheEmptyStringImpl);
    }

    return *s_theEmptyStringImpl;
}

StringImpl::StringImpl(ConstructWithInlineBufferTag, size_t length)
    : m_length(length) { }

StringImpl::~StringImpl() { }

NonNullReferencePointer<StringImpl> StringImpl::createUninitialized(size_t length, char*& buffer) {

    VERIFY(length);
    
    void* slot = kmalloc(allocationSizeForStringImpl(length));
    
    VERIFY(slot);
    
    auto new_stringimpl = adoptReference(*new (slot) StringImpl(ConstructWithInlineBuffer, length));
    
    buffer = const_cast<char*>(new_stringimpl->characters());
    
    buffer[length] = '\0';
    
    return new_stringimpl;
}

ReferencePointer<StringImpl> StringImpl::create(char const* cstring, size_t length, ShouldChomp should_chomp)
{
    if (!cstring) {

        return nullptr;
    }

    if (should_chomp) {

        while (length) {

            char last_ch = cstring[length - 1];

            if (!last_ch || last_ch == '\n' || last_ch == '\r') {

                --length;
            }
            else {

                break;
            }
        }
    }

    if (!length) {

        return theEmptyStringImpl();
    }

    char* buffer;
    auto new_stringimpl = createUninitialized(length, buffer);
    memcpy(buffer, cstring, length * sizeof(char));

    return new_stringimpl;
}

ReferencePointer<StringImpl> StringImpl::create(char const* cstring, ShouldChomp shouldChomp) {

    if (!cstring) {

        return nullptr;
    }

    if (!*cstring) {

        return theEmptyStringImpl();
    }

    return create(cstring, strlen(cstring), shouldChomp);
}

ReferencePointer<StringImpl> StringImpl::create(ReadOnlyBytes bytes, ShouldChomp shouldChomp) {

    return StringImpl::create(reinterpret_cast<char const*>(bytes.data()), bytes.size(), shouldChomp);
}

ReferencePointer<StringImpl> StringImpl::create_lowercased(char const* cstring, size_t length) {

    if (!cstring) {

        return nullptr;
    }

    if (!length) {

        return theEmptyStringImpl();
    }

    char* buffer;
    
    auto impl = createUninitialized(length, buffer);
    
    for (size_t i = 0; i < length; ++i) {

        buffer[i] = (char)toAsciiLowercase(cstring[i]);
    }

    return impl;
}

ReferencePointer<StringImpl> StringImpl::create_uppercased(char const* cstring, size_t length) {

    if (!cstring) {

        return nullptr;
    }

    if (!length) {

        return theEmptyStringImpl();
    }

    char* buffer;
    
    auto impl = createUninitialized(length, buffer);
    
    for (size_t i = 0; i < length; ++i) {

        buffer[i] = (char)toAsciiUppercase(cstring[i]);
    }

    return impl;
}

NonNullReferencePointer<StringImpl> StringImpl::to_lowercase() const {

    for (size_t i = 0; i < m_length; ++i) {

        if (isAsciiUpperAlpha(characters()[i])) {

            return create_lowercased(characters(), m_length).releaseNonNull();
        }
    }

    return const_cast<StringImpl&>(*this);
}

NonNullReferencePointer<StringImpl> StringImpl::to_uppercase() const {

    for (size_t i = 0; i < m_length; ++i) {

        if (isAsciiLowerAlpha(characters()[i])) {

            return create_uppercased(characters(), m_length).releaseNonNull();
        }
    }

    return const_cast<StringImpl&>(*this);
}

unsigned StringImpl::caseInsensitiveHash() const {

    return caseInsensitiveStringHash(characters(), length());
}

void StringImpl::compute_hash() const
{
    if (!length())
        m_hash = 0;
    else
        m_hash = stringHash(characters(), m_length);
    m_has_hash = true;
}