/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Format.h"
#include "Forward.h"
#include "ReferencePointer.h"
#include "StringImpl.h"
#include "StringUtils.h"
#include "Traits.h"

// String is a convenience wrapper around StringImpl, suitable for passing
// around as a value type. It's basically the same as passing around a
// ReferencePointer<StringImpl>, with a bit of syntactic sugar.
//
// Note that StringImpl is an immutable object that cannot shrink or grow.
// Its allocation size is snugly tailored to the specific string it contains.
// Copying a String is very efficient, since the internal StringImpl is
// retainable and so copying only requires modifying the ref count.
//
// There are three main ways to construct a new String:
//
//     s = String("some literal");
//
//     s = String::formatted("{} little piggies", m_piggies);
//
//     StringBuilder builder;
//     builder.append("abc");
//     builder.append("123");
//     s = builder.toString();

class String {

public:

    ~String() = default;

    String() = default;

    String(StringView view)
        : m_impl(StringImpl::create(view.charactersWithoutNullTermination(), view.length())) { }

    String(String const& other)
        : m_impl(const_cast<String&>(other).m_impl) { }

    String(String&& other)
        : m_impl(move(other.m_impl)) { }

    String(char const* cstring, ShouldChomp shouldChomp = NoChomp)
        : m_impl(StringImpl::create(cstring, shouldChomp)) { }

    String(char const* cstring, size_t length, ShouldChomp shouldChomp = NoChomp)
        : m_impl(StringImpl::create(cstring, length, shouldChomp)) { }

    explicit String(ReadOnlyBytes bytes, ShouldChomp shouldChomp = NoChomp)
        : m_impl(StringImpl::create(bytes, shouldChomp)) { }

    String(StringImpl const& impl)
        : m_impl(const_cast<StringImpl&>(impl)) { }

    String(StringImpl const* impl)
        : m_impl(const_cast<StringImpl*>(impl)) { }

    String(ReferencePointer<StringImpl>&& impl)
        : m_impl(move(impl)) { }

    String(NonNullReferencePointer<StringImpl>&& impl)
        : m_impl(move(impl)) { }

    [[nodiscard]] static String repeated(char, size_t count);
    [[nodiscard]] static String repeated(StringView, size_t count);

    [[nodiscard]] static String bijectiveBaseFrom(size_t value, unsigned base = 26, StringView map = {});
    [[nodiscard]] static String romanNumberFrom(size_t value);

    [[nodiscard]] bool matches(StringView mask, CaseSensitivity = CaseSensitivity::CaseInsensitive) const;
    [[nodiscard]] bool matches(StringView mask, Vector<MaskSpan>&, CaseSensitivity = CaseSensitivity::CaseInsensitive) const;

    template<typename T = int>
    [[nodiscard]] Optional<T> to_int(TrimWhitespace = TrimWhitespace::Yes) const;
    template<typename T = unsigned>
    [[nodiscard]] Optional<T> to_uint(TrimWhitespace = TrimWhitespace::Yes) const;

    [[nodiscard]] String to_lowercase() const;
    [[nodiscard]] String to_uppercase() const;
    [[nodiscard]] String to_snakecase() const;
    [[nodiscard]] String to_titlecase() const;

    [[nodiscard]] bool isWhitespace() const { return StringUtils::isWhitespace(*this); }

    [[nodiscard]] String trim(StringView characters, TrimMode mode = TrimMode::Both) const
    {
        auto trimmed_view = StringUtils::trim(view(), characters, mode);
        if (view() == trimmed_view)
            return *this;
        return trimmed_view;
    }

    [[nodiscard]] String trim_whitespace(TrimMode mode = TrimMode::Both) const
    {
        auto trimmed_view = StringUtils::trim_whitespace(view(), mode);
        if (view() == trimmed_view)
            return *this;
        return trimmed_view;
    }

    [[nodiscard]] bool equalsIgnoringCase(StringView) const;

    [[nodiscard]] bool contains(StringView, CaseSensitivity = CaseSensitivity::CaseSensitive) const;
    [[nodiscard]] bool contains(char, CaseSensitivity = CaseSensitivity::CaseSensitive) const;

    [[nodiscard]] Vector<String> split_limit(char separator, size_t limit, bool keep_empty = false) const;
    [[nodiscard]] Vector<String> split(char separator, bool keep_empty = false) const;
    [[nodiscard]] Vector<StringView> splitView(char separator, bool keep_empty = false) const;
    [[nodiscard]] Vector<StringView> splitView(Function<bool(char)> separator, bool keep_empty = false) const;

    [[nodiscard]] Optional<size_t> find(char needle, size_t start = 0) const { return StringUtils::find(*this, needle, start); }
    [[nodiscard]] Optional<size_t> find(StringView needle, size_t start = 0) const { return StringUtils::find(*this, needle, start); }
    [[nodiscard]] Optional<size_t> find_last(char needle) const { return StringUtils::find_last(*this, needle); }
    // FIXME: Implement find_last(StringView) for API symmetry.
    Vector<size_t> find_all(StringView needle) const;
    using SearchDirection = StringUtils::SearchDirection;
    [[nodiscard]] Optional<size_t> find_any_of(StringView needles, SearchDirection direction) const { return StringUtils::find_any_of(*this, needles, direction); }

    [[nodiscard]] String substring(size_t start, size_t length) const;
    [[nodiscard]] String substring(size_t start) const;
    [[nodiscard]] StringView substringView(size_t start, size_t length) const;
    [[nodiscard]] StringView substringView(size_t start) const;

    [[nodiscard]] bool isNull() const { return !m_impl; }
    [[nodiscard]] ALWAYS_INLINE bool isEmpty() const { return length() == 0; }
    [[nodiscard]] ALWAYS_INLINE size_t length() const { return m_impl ? m_impl->length() : 0; }
    // Includes NUL-terminator, if non-nullptr.
    [[nodiscard]] ALWAYS_INLINE char const* characters() const { return m_impl ? m_impl->characters() : nullptr; }

    [[nodiscard]] bool copy_characters_to_buffer(char* buffer, size_t buffer_size) const;

    [[nodiscard]] ALWAYS_INLINE ReadOnlyBytes bytes() const
    {
        if (m_impl) {
            return m_impl->bytes();
        }
        return {};
    }

    [[nodiscard]] ALWAYS_INLINE char const& operator[](size_t i) const {

        VERIFY(!isNull());
        
        return (*m_impl)[i];
    }

    using ConstIterator = SimpleIterator<const String, char const>;

    [[nodiscard]] constexpr ConstIterator begin() const { return ConstIterator::begin(*this); }
    [[nodiscard]] constexpr ConstIterator end() const { return ConstIterator::end(*this); }

    [[nodiscard]] bool startsWith(StringView, CaseSensitivity = CaseSensitivity::CaseSensitive) const;
    [[nodiscard]] bool endsWith(StringView, CaseSensitivity = CaseSensitivity::CaseSensitive) const;
    [[nodiscard]] bool startsWith(char) const;
    [[nodiscard]] bool endsWith(char) const;

    bool operator==(String const&) const;
    bool operator!=(String const& other) const { return !(*this == other); }

    bool operator==(StringView) const;
    bool operator!=(StringView other) const { return !(*this == other); }

    bool operator<(String const&) const;
    bool operator<(char const*) const;
    bool operator>=(String const& other) const { return !(*this < other); }
    bool operator>=(char const* other) const { return !(*this < other); }

    bool operator>(String const&) const;
    bool operator>(char const*) const;
    bool operator<=(String const& other) const { return !(*this > other); }
    bool operator<=(char const* other) const { return !(*this > other); }

    bool operator==(char const* cstring) const;
    bool operator!=(char const* cstring) const { return !(*this == cstring); }

    [[nodiscard]] String isolated_copy() const;

    [[nodiscard]] static String empty()
    {
        return StringImpl::theEmptyStringImpl();
    }

    [[nodiscard]] StringImpl* impl() { return m_impl.pointer(); }
    [[nodiscard]] StringImpl const* impl() const { return m_impl.pointer(); }

    String& operator=(String&& other)
    {
        if (this != &other)
            m_impl = move(other.m_impl);
        return *this;
    }

    String& operator=(String const& other)
    {
        if (this != &other)
            m_impl = const_cast<String&>(other).m_impl;
        return *this;
    }

    String& operator=(std::nullptr_t)
    {
        m_impl = nullptr;
        return *this;
    }

    String& operator=(ReadOnlyBytes bytes)
    {
        m_impl = StringImpl::create(bytes);
        return *this;
    }

    [[nodiscard]] UInt32 hash() const
    {
        if (!m_impl)
            return 0;
        return m_impl->hash();
    }

    template<typename BufferType>
    [[nodiscard]] static String copy(BufferType const& buffer, ShouldChomp should_chomp = NoChomp)
    {
        if (buffer.isEmpty())
            return empty();
        return String((char const*)buffer.data(), buffer.size(), should_chomp);
    }

    [[nodiscard]] static String vformatted(StringView fmtstr, TypeErasedFormatParams&);

    template<typename... Parameters>
    [[nodiscard]] static String formatted(CheckedFormatString<Parameters...>&& fmtstr, Parameters const&... parameters) {

        VariadicFormatParams variadic_format_parameters { parameters... };
        
        return vformatted(fmtstr.view(), variadic_format_parameters);
    }

    template<typename T>
    [[nodiscard]] static String number(T value) requires IsArithmetic<T> {

        return formatted("{}", value);
    }

    [[nodiscard]] StringView view() const {

        return { characters(), length() };
    }

    [[nodiscard]] String replace(StringView needle, StringView replacement, bool all_occurrences = false) const { return StringUtils::replace(*this, needle, replacement, all_occurrences); }
    [[nodiscard]] size_t count(StringView needle) const { return StringUtils::count(*this, needle); }
    [[nodiscard]] String reverse() const;

    template<typename... Ts>
    [[nodiscard]] ALWAYS_INLINE constexpr bool isOneOf(Ts&&... strings) const {

        return (... || this->operator==(forward<Ts>(strings)));
    }

    template<typename... Ts>
    [[nodiscard]] ALWAYS_INLINE constexpr bool isOneOfIgnoringCase(Ts&&... strings) const {

        return (... ||
                [this, &strings]() -> bool {
            if constexpr (requires(Ts a) { a.view()->StringView; }) {

                return this->equalsIgnoringCase(forward<Ts>(strings.view()));
            }
            else {

                return this->equalsIgnoringCase(forward<Ts>(strings));
            }
        }());
    }

    String& operator+=(String const&);

private:

    ReferencePointer<StringImpl> m_impl;
};

template<>
struct Traits<String> : public GenericTraits<String> {

    static unsigned hash(String const& s) { return s.impl() ? s.impl()->hash() : 0; }
};

struct CaseInsensitiveStringTraits : public Traits<String> {

    static unsigned hash(String const& s) { return s.impl() ? s.impl()->caseInsensitiveHash() : 0; }
    
    static bool equals(String const& a, String const& b) { return a.equalsIgnoringCase(b); }
};

bool operator<(char const*, String const&);
bool operator>=(char const*, String const&);
bool operator>(char const*, String const&);
bool operator<=(char const*, String const&);

String operator+(String const&, String const&);

String escape_html_entities(StringView html);

template<typename T>
struct Formatter<NonNullReferencePointer<T>> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, NonNullReferencePointer<T> const& value)
    {
        auto str = String::formatted("{}", *value);
        return Formatter<StringView>::format(builder, str);
    }
};
