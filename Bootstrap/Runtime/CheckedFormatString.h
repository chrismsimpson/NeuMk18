/*
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "AllOf.h"
#include "AnyOf.h"
#include "LinearArray.h"
#include "StdLibExtras.h"
#include "StringView.h"

#ifdef ENABLE_COMPILETIME_FORMAT_CHECK
// FIXME: Seems like clang doesn't like calling 'consteval' functions inside 'consteval' functions quite the same way as GCC does,
//        it seems to entirely forget that it accepted that parameters to a 'consteval' function to begin with.
#    if defined(__clang__) || defined(__CLION_IDE_)
#        undef ENABLE_COMPILETIME_FORMAT_CHECK
#    endif
#endif

#ifdef ENABLE_COMPILETIME_FORMAT_CHECK

namespace Format::Detail {

    // We have to define a local "purely constexpr" Array that doesn't lead back to us (via e.g. VERIFY)
    template<typename T, size_t Size>
    struct Array {

        constexpr static size_t size() { return Size; }
        
        constexpr const T& operator[](size_t index) const { return __data[index]; }
        
        constexpr T& operator[](size_t index) { return __data[index]; }
        
        using ConstIterator = SimpleIterator<const Array, const T>;
        using Iterator = SimpleIterator<Array, T>;

        constexpr ConstIterator begin() const { return ConstIterator::begin(*this); }
        constexpr Iterator begin() { return Iterator::begin(*this); }

        constexpr ConstIterator end() const { return ConstIterator::end(*this); }
        constexpr Iterator end() { return Iterator::end(*this); }

        T __data[Size];
    };

    template<typename... Args>
    void compileTimeFail(Args...);

    template<size_t N>
    consteval auto extractUsedArgumentIndex(char const (&fmt)[N], size_t specifierStartIndex, size_t specifierEndIndex, size_t& nextImplicitArgumentIndex) {

        struct {

            size_t indexValue { 0 };

            bool sawExplicitIndex { false };

        } state;

        for (size_t i = specifierStartIndex; i < specifierEndIndex; ++i) {

            auto c = fmt[i];

            if (c > '9' || c < '0') {

                break;
            }

            state.indexValue *= 10;
            state.indexValue += c - '0';
            state.sawExplicitIndex = true;
        }

        if (!state.sawExplicitIndex) {

            return nextImplicitArgumentIndex++;
        }

        return state.indexValue;
    }

    // FIXME: We should rather parse these format strings at compile-time if possible.
    
    template<size_t N>
    consteval auto countFormatParameters(char const (&fmt)[N]) {

        struct {

            // FIXME: Switch to variable-sized storage whenever we can come up with one :)
            
            LinearArray<size_t, 128> usedArguments { 0 };
            
            size_t total_used_argument_count { 0 };
            size_t nextImplicitArgumentIndex { 0 };
            bool has_explicit_argument_references { false };

            size_t unclosed_braces { 0 };
            size_t extra_closed_braces { 0 };
            size_t nesting_level { 0 };

            LinearArray<size_t, 4> last_format_specifier_start { 0 };
            size_t total_used_last_format_specifier_start_count { 0 };

        } result;

        for (size_t i = 0; i < N; ++i) {

            auto ch = fmt[i];

            switch (ch) {

            case '{':

                if (i + 1 < N && fmt[i + 1] == '{') {

                    ++i;

                    continue;
                }

                // Note: There's no compile-time throw, so we have to abuse a compile-time string to store errors.
                
                if (result.total_used_last_format_specifier_start_count >= result.last_format_specifier_start.size() - 1) {

                    compileTimeFail("Format-String Checker internal error: Format specifier nested too deep");
                }

                result.last_format_specifier_start[result.total_used_last_format_specifier_start_count++] = i + 1;

                ++result.unclosed_braces;
                ++result.nesting_level;

                break;

            ///

            case '}':

                if (result.nesting_level == 0) {

                    if (i + 1 < N && fmt[i + 1] == '}') {

                        ++i;

                        continue;
                    }
                }

                if (result.unclosed_braces) {

                    --result.nesting_level;
                    --result.unclosed_braces;

                    if (result.total_used_last_format_specifier_start_count == 0) {

                        compileTimeFail("Format-String Checker internal error: Expected location information");
                    }

                    auto const specifierStartIndex = result.last_format_specifier_start[--result.total_used_last_format_specifier_start_count];

                    if (result.total_used_argument_count >= result.usedArguments.size()) {

                        compileTimeFail("Format-String Checker internal error: Too many format arguments in format string");
                    }

                    auto usedArgumentIndex = extractUsedArgumentIndex<N>(fmt, specifierStartIndex, i, result.nextImplicitArgumentIndex);
                    
                    if (usedArgumentIndex + 1 != result.nextImplicitArgumentIndex) {

                        result.has_explicit_argument_references = true;
                    }

                    result.usedArguments[result.total_used_argument_count++] = usedArgumentIndex;

                }
                else {

                    ++result.extra_closed_braces;
                }

                break;

            ///

            default:

                continue;
            }
        }
        
        return result;
    }
}

#endif

namespace Format::Detail {
        
    template<typename... Args>
    struct CheckedFormatString {

        template<size_t N>
        consteval CheckedFormatString(char const (&fmt)[N])
            : m_string { fmt }
        {
    #ifdef ENABLE_COMPILETIME_FORMAT_CHECK
            check_format_parameter_consistency<N, sizeof...(Args)>(fmt);
    #endif
        }

        template<typename T>
        CheckedFormatString(const T& unchecked_fmt) requires(requires(T t) { StringView { t }; })
            : m_string(unchecked_fmt) { }

        auto view() const { return m_string; }

    private:

    #ifdef ENABLE_COMPILETIME_FORMAT_CHECK

        template<size_t N, size_t param_count>
        consteval static bool check_format_parameter_consistency(char const (&fmt)[N]) {

            auto check = countFormatParameters<N>(fmt);

            if (check.unclosed_braces != 0) {

                compileTimeFail("Extra unclosed braces in format string");
            }

            if (check.extra_closed_braces != 0) {

                compileTimeFail("Extra closing braces in format string");
            }

            {
                auto begin = check.usedArguments.begin();
                
                auto end = check.usedArguments.begin() + check.total_used_argument_count;
                
                auto has_all_referenced_arguments = !anyOf(begin, end, [](auto& entry) { return entry >= param_count; });

                if (!has_all_referenced_arguments) {

                    compileTimeFail("Format string references nonexistent parameter");
                }
            }

            if (!check.has_explicit_argument_references && check.total_used_argument_count != param_count) {

                compileTimeFail("Format string does not reference all passed parameters");
            }

            // Ensure that no passed parameter is ignored or otherwise not referenced in the format
            // As this check is generally pretty expensive, try to avoid it where it cannot fail.
            // We will only do this check if the format string has explicit argument refs
            // otherwise, the check above covers this check too, as implicit refs
            // monotonically increase, and cannot have 'gaps'.
            
            if (check.has_explicit_argument_references) {

                auto all_parameters = iota_LinearArray<size_t, param_count>(0);

                constexpr auto contains = [](auto begin, auto end, auto entry) {

                    for (; begin != end; begin++) {
                        if (*begin == entry) {
                            
                        }
                            return true;
                    }

                    return false;
                };

                auto references_all_arguments = allOf(
                    all_parameters,
                    [&](auto& entry) {
                        return contains(
                            check.usedArguments.begin(),
                            check.usedArguments.begin() + check.total_used_argument_count,
                            entry);
                    });

                if (!references_all_arguments) {

                    compileTimeFail("Format string does not reference all passed parameters");
                }
            }

            return true;
        }

    #endif

        StringView m_string;
    };
}

template<typename... Args>
using CheckedFormatString = Format::Detail::CheckedFormatString<IdentityType<Args>...>;
