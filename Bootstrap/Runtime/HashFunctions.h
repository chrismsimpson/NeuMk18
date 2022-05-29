/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Types.h"

constexpr unsigned UInt32Hash(UInt32 key) {

    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

constexpr unsigned doubleHash(UInt32 key) {

    unsigned const magic = 0xBA5EDB01;

    if (key == magic) {

        return 0u;
    }

    if (key == 0u) {

        key = magic;
    }

    key ^= key << 13;
    key ^= key >> 17;
    key ^= key << 5;
    return key;
}

constexpr unsigned pairUInt32Hash(UInt32 key1, UInt32 key2) {

    return UInt32Hash((UInt32Hash(key1) * 209) ^ (UInt32Hash(key2 * 413)));
}

constexpr unsigned UInt64Hash(UInt64 key) {

    UInt32 first = key & 0xFFFFFFFF;
    UInt32 last = key >> 32;
    return pairUInt32Hash(first, last);
}

constexpr unsigned pointerHash(FlatPointer ptr) {

    if constexpr (sizeof(ptr) == 8) {

        return UInt64Hash(ptr);
    }
    else {

        return UInt32Hash(ptr);
    }
}

inline unsigned pointerHash(void const* ptr) {

    return pointerHash(FlatPointer(ptr));
}
