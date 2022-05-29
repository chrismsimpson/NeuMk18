/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "AnyOf.h"
#include "Find.h"
#include "Function.h"
#include "Memory.h"
#include "StringView.h"
#include "Vector.h"

#ifndef KERNEL
#    include "String.h"
#endif

#ifndef KERNEL
StringView::StringView(String const& string)
    : m_characters(string.characters())
    , m_length(string.length())
{
}
#endif

Vector<StringView> StringView::split_view(char const separator, bool keep_empty) const
{
    StringView seperator_view { &separator, 1 };
    return split_view(seperator_view, keep_empty);
}

Vector<StringView> StringView::split_view(StringView separator, bool keep_empty) const
{
    Vector<StringView> parts;
    for_each_split_view(separator, keep_empty, [&](StringView view) {
        parts.append(view);
    });
    return parts;
}

Vector<StringView> StringView::lines(bool consider_cr) const {

    if (isEmpty()) {

        return { };
    }

    if (!consider_cr) {

        return split_view('\n', true);
    }

    Vector<StringView> v;
    
    size_t substart = 0;
    
    bool last_ch_was_cr = false;
    
    bool split_view = false;
    
    for (size_t i = 0; i < length(); ++i) { 

        char ch = charactersWithoutNullTermination()[i];

        if (ch == '\n') {

            split_view = true;

            if (last_ch_was_cr) {

                substart = i + 1;
                
                split_view = false;
            }
        }

        if (ch == '\r') {
            
            split_view = true;
            
            last_ch_was_cr = true;
        } 
        else {
            
            last_ch_was_cr = false;
        }

        if (split_view) {
            
            size_t sublen = i - substart;
            
            v.append(substring_view(substart, sublen));
            
            substart = i + 1;
        }

        split_view = false;
    }

    size_t taillen = length() - substart;
    
    if (taillen != 0) {

        v.append(substring_view(substart, taillen));
    }
    
    return v;
}

bool StringView::starts_with(char ch) const {

    if (isEmpty()) {

        return false;
    }

    return ch == charactersWithoutNullTermination()[0];
}

bool StringView::starts_with(StringView str, CaseSensitivity case_sensitivity) const {

    return StringUtils::starts_with(*this, str, case_sensitivity);
}

bool StringView::ends_with(char ch) const {

    if (isEmpty()) {

        return false;
    }

    return ch == charactersWithoutNullTermination()[length() - 1];
}

bool StringView::ends_with(StringView str, CaseSensitivity case_sensitivity) const {

    return StringUtils::ends_with(*this, str, case_sensitivity);
}

bool StringView::matches(StringView mask, Vector<MaskSpan>& mask_spans, CaseSensitivity case_sensitivity) const {

    return StringUtils::matches(*this, mask, case_sensitivity, &mask_spans);
}

bool StringView::matches(StringView mask, CaseSensitivity case_sensitivity) const {

    return StringUtils::matches(*this, mask, case_sensitivity);
}

bool StringView::contains(char needle) const {

    for (char current : *this) { 

        if (current == needle) {

            return true;
        }
    }

    return false;
}

bool StringView::contains(StringView needle, CaseSensitivity case_sensitivity) const {

    return StringUtils::contains(*this, needle, case_sensitivity);
}

bool StringView::equals_ignoring_case(StringView other) const {

    return StringUtils::equals_ignoring_case(*this, other);
}

#ifndef KERNEL

String StringView::to_lowercase_string() const {

    return StringImpl::create_lowercased(charactersWithoutNullTermination(), length());
}

String StringView::to_uppercase_string() const {

    return StringImpl::create_uppercased(charactersWithoutNullTermination(), length());
}

String StringView::to_titlecase_string() const {

    return StringUtils::to_titlecase(*this);
}

#endif

StringView StringView::substring_view_starting_from_substring(StringView substring) const {

    char const* remainingCharacters = substring.charactersWithoutNullTermination();
    
    VERIFY(remainingCharacters >= m_characters);
    
    VERIFY(remainingCharacters <= m_characters + m_length);
    
    size_t remainingLength = m_length - (remainingCharacters - m_characters);
    
    return { remainingCharacters, remainingLength };
}

StringView StringView::substringViewStartingAfterSubstring(StringView substring) const {

    char const* remainingCharacters = substring.charactersWithoutNullTermination() + substring.length();
    
    VERIFY(remainingCharacters >= m_characters);
    
    VERIFY(remainingCharacters <= m_characters + m_length);
    
    size_t remainingLength = m_length - (remainingCharacters - m_characters);
    
    return { remainingCharacters, remainingLength };
}

bool StringView::copy_characters_to_buffer(char* buffer, size_t buffer_size) const
{
    // We must fit at least the NUL-terminator.
    VERIFY(buffer_size > 0);

    size_t characters_to_copy = min(m_length, buffer_size - 1);
    __builtin_memcpy(buffer, m_characters, characters_to_copy);
    buffer[characters_to_copy] = 0;

    return characters_to_copy == m_length;
}

template<typename T>
Optional<T> StringView::to_int() const {

    return StringUtils::convert_to_int<T>(*this);
}

template Optional<Int8> StringView::to_int() const;
template Optional<Int16> StringView::to_int() const;
template Optional<Int32> StringView::to_int() const;
template Optional<long> StringView::to_int() const;
template Optional<long long> StringView::to_int() const;

template<typename T>
Optional<T> StringView::to_uint() const {

    return StringUtils::convert_to_uint<T>(*this);
}

template Optional<UInt8> StringView::to_uint() const;
template Optional<UInt16> StringView::to_uint() const;
template Optional<UInt32> StringView::to_uint() const;
template Optional<unsigned long> StringView::to_uint() const;
template Optional<unsigned long long> StringView::to_uint() const;
template Optional<long> StringView::to_uint() const;
template Optional<long long> StringView::to_uint() const;

#ifndef KERNEL

bool StringView::operator==(String const& string) const {

    return *this == string.view();
}

String StringView::toString() const { return String { *this }; }

String StringView::replace(StringView needle, StringView replacement, bool all_occurrences) const {

    return StringUtils::replace(*this, needle, replacement, all_occurrences);
}

#endif

Vector<size_t> StringView::find_all(StringView needle) const {

    return StringUtils::find_all(*this, needle);
}

Vector<StringView> StringView::split_view_if(Function<bool(char)> const& predicate, bool keep_empty) const {

    if (isEmpty()) {

        return { };
    }

    Vector<StringView> v;

    size_t substart = 0;

    for (size_t i = 0; i < length(); ++i) {

        char ch = charactersWithoutNullTermination()[i];
        
        if (predicate(ch)) {
            
            size_t sublen = i - substart;
            
            if (sublen != 0 || keep_empty) {

                v.append(substring_view(substart, sublen));
            }
            
            substart = i + 1;
        }
    }

    size_t taillen = length() - substart;

    if (taillen != 0 || keep_empty) {

        v.append(substring_view(substart, taillen));
    }

    return v;
}

