/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Checked.h"
#include "StdLibExtras.h"
#include "StringBuilder.h"
#include "StringView.h"
#include "UnicodeUtils.h"

inline ErrorOr<void> StringBuilder::will_append(size_t size)
{
    TRY(m_buffer.add_capacity(size));
    return {};
}

StringBuilder::StringBuilder()
{
}

ErrorOr<void> StringBuilder::tryAppend(StringView string)
{
    if (string.isEmpty())
        return {};
    TRY(will_append(string.length()));
    TRY(m_buffer.push_values((u8 const*)string.charactersWithoutNullTermination(), string.length()));
    return {};
}

ErrorOr<void> StringBuilder::tryAppend(char ch)
{
    TRY(will_append(1));
    TRY(m_buffer.push(ch));
    return {};
}

void StringBuilder::append(StringView string)
{
    MUST(tryAppend(string));
}

ErrorOr<void> StringBuilder::tryAppend(char const* characters, size_t length)
{
    return tryAppend(StringView { characters, length });
}

void StringBuilder::append(char const* characters, size_t length)
{
    MUST(tryAppend(characters, length));
}

void StringBuilder::append(char ch)
{
    MUST(tryAppend(ch));
}

String StringBuilder::to_string() const
{
    if (isEmpty())
        return String::empty();
    return String((char const*)data(), length());
}

String StringBuilder::build() const
{
    return to_string();
}

StringView StringBuilder::string_view() const
{
    return StringView { data(), m_buffer.size() };
}

void StringBuilder::clear()
{
    static_cast<void>(m_buffer.resize(0));
}

ErrorOr<void> StringBuilder::try_append_code_point(u32 code_point)
{
    auto nwritten = UnicodeUtils::code_point_to_utf8(code_point, [this](char c) { append(c); });
    if (nwritten < 0) {
        TRY(tryAppend(0xef));
        TRY(tryAppend(0xbf));
        TRY(tryAppend(0xbd));
    }
    return {};
}

void StringBuilder::append_code_point(u32 code_point)
{
    MUST(try_append_code_point(code_point));
}

void StringBuilder::append_as_lowercase(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        append(ch + 0x20);
    else
        append(ch);
}

void StringBuilder::append_escaped_for_json(StringView string)
{
    MUST(try_append_escaped_for_json(string));
}

ErrorOr<void> StringBuilder::try_append_escaped_for_json(StringView string)
{
    for (auto ch : string) {
        switch (ch) {
        case '\b':
            TRY(tryAppend("\\b"));
            break;
        case '\n':
            TRY(tryAppend("\\n"));
            break;
        case '\t':
            TRY(tryAppend("\\t"));
            break;
        case '\"':
            TRY(tryAppend("\\\""));
            break;
        case '\\':
            TRY(tryAppend("\\\\"));
            break;
        default:
            if (ch >= 0 && ch <= 0x1f)
                TRY(try_appendff("\\u{:04x}", ch));
            else
                TRY(tryAppend(ch));
        }
    }
    return {};
}
