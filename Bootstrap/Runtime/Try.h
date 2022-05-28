/*
 * Copyright (c) 2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// NOTE: These macros work with any result type that has the expected APIs.
//       It's designed with ErrorOr in mind.

#define TRY(...)                                      \
    ({                                                \
        auto _temporary_result = (__VA_ARGS__);       \
        if (_temporary_result.isError()) {            \
            return _temporary_result.releaseError();  \
        }                                             \
        _temporary_result.releaseValue();             \
    })

#define MUST(...)                               \
    ({                                          \
        auto _temporary_result = (__VA_ARGS__); \
        VERIFY(!_temporary_result.isError());   \
        _temporary_result.releaseValue();       \
    })
