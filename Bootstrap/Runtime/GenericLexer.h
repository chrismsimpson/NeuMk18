/*
 * Copyright (c) 2020, Benoit Lormeau <blormeau@outlook.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "StringView.h"

class GenericLexer {

public:

    constexpr explicit GenericLexer(StringView input)
        : m_input(input) { }

    constexpr size_t tell() const { return m_index; }
    constexpr size_t tell_remaining() const { return m_input.length() - m_index; }

    StringView remaining() const { return m_input.substring_view(m_index); }

    constexpr bool isEof() const { return m_index >= m_input.length(); }

    constexpr char peek(size_t offset = 0) const {

        return (m_index + offset < m_input.length()) ? m_input[m_index + offset] : '\0';
    }

    constexpr bool next_is(char expected) const {

        return peek() == expected;
    }

    constexpr bool next_is(StringView expected) const {

        for (size_t i = 0; i < expected.length(); ++i) {

            if (peek(i) != expected[i]) {

                return false;
            }
        }

        return true;
    }

    constexpr bool next_is(char const* expected) const {

        for (size_t i = 0; expected[i] != '\0'; ++i) {

            if (peek(i) != expected[i]) {

                return false;
            }
        }

        return true;
    }

    constexpr void retreat() {

        VERIFY(m_index > 0);

        --m_index;
    }

    constexpr void retreat(size_t count) {

        VERIFY(m_index >= count);

        m_index -= count;
    }

    constexpr char consume()
    {
        VERIFY(!isEof());
        return m_input[m_index++];
    }

    template<typename T>
    constexpr bool consumeSpecific(const T& next)
    {
        if (!next_is(next))
            return false;

        if constexpr (requires { next.length(); }) {
            ignore(next.length());
        } else {
            ignore(sizeof(next));
        }
        return true;
    }

#ifndef KERNEL

    bool consumeSpecific(String const& next) {

        return consumeSpecific(StringView { next });
    }

#endif

    constexpr bool consumeSpecific(char const* next) {

        return consumeSpecific(StringView { next });
    }

    constexpr char consume_escaped_character(char escape_char = '\\', StringView escape_map = "n\nr\rt\tb\bf\f") {

        if (!consumeSpecific(escape_char)) {

            return consume();
        }

        auto c = consume();

        for (size_t i = 0; i < escape_map.length(); i += 2) {
            if (c == escape_map[i])
                return escape_map[i + 1];
        }

        return c;
    }

    StringView consume(size_t count);
    StringView consume_all();
    StringView consume_line();
    StringView consume_until(char);
    StringView consume_until(char const*);
    StringView consume_until(StringView);
    StringView consume_quoted_string(char escape_char = 0);

    constexpr void ignore(size_t count = 1) {

        count = min(count, m_input.length() - m_index);
        
        m_index += count;
    }

    constexpr void ignore_until(char stop) {

        while (!isEof() && peek() != stop) {

            ++m_index;
        }

        ignore();
    }

    constexpr void ignore_until(char const* stop) {

        while (!isEof() && !next_is(stop)) {

            ++m_index;
        }

        ignore(__builtin_strlen(stop));
    }

    /*
     * Conditions are used to match arbitrary characters. You can use lambdas,
     * ctype functions, or is_any_of() and its derivatives (see below).
     * A few examples:
     *   - `if (lexer.next_is(isdigit))`
     *   - `auto name = lexer.consume_while([](char c) { return isalnum(c) || c == '_'; });`
     *   - `lexer.ignore_until(is_any_of("<^>"));`
     */

    // Test the next character against a Condition
    template<typename TPredicate>
    constexpr bool next_is(TPredicate pred) const {

        return pred(peek());
    }

    // Consume and return characters while `pred` returns true
    template<typename TPredicate>
    StringView consume_while(TPredicate pred) {

        size_t start = m_index;

        while (!isEof() && pred(peek())) {

            ++m_index;
        }

        size_t length = m_index - start;

        if (length == 0) {

            return { };
        }

        return m_input.substring_view(start, length);
    }

    // Consume and return characters until `pred` return true
    template<typename TPredicate>
    StringView consume_until(TPredicate pred) {

        size_t start = m_index;

        while (!isEof() && !pred(peek())) {

            ++m_index;
        }

        size_t length = m_index - start;

        if (length == 0) {

            return { };
        }

        return m_input.substring_view(start, length);
    }

    // Ignore characters while `pred` returns true
    template<typename TPredicate>
    constexpr void ignore_while(TPredicate pred) {

        while (!isEof() && pred(peek())) {

            ++m_index;
        }
    }

    // Ignore characters until `pred` return true
    // We don't skip the stop character as it may not be a unique value
    template<typename TPredicate>
    constexpr void ignore_until(TPredicate pred) {

        while (!isEof() && !pred(peek())) {

            ++m_index;
        }
    }

protected:
    StringView m_input;
    size_t m_index { 0 };
};

constexpr auto is_any_of(StringView values)
{
    return [values](auto c) { return values.contains(c); };
}

constexpr auto is_not_any_of(StringView values)
{
    return [values](auto c) { return !values.contains(c); };
}

constexpr auto is_path_separator = is_any_of("/\\");
constexpr auto is_quote = is_any_of("'\"");
