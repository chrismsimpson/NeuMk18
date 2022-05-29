/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Types.h"

constexpr unsigned int_hash(UInt32 key)
{
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

constexpr unsigned double_hash(UInt32 key)
{
    unsigned const magic = 0xBA5EDB01;
    if (key == magic)
        return 0u;
    if (key == 0u)
        key = magic;

    key ^= key << 13;
    key ^= key >> 17;
    key ^= key << 5;
    return key;
}

constexpr unsigned pair_int_hash(UInt32 key1, UInt32 key2)
{
    return int_hash((int_hash(key1) * 209) ^ (int_hash(key2 * 413)));
}

constexpr unsigned u64_hash(UInt64 key)
{
    UInt32 first = key & 0xFFFFFFFF;
    UInt32 last = key >> 32;
    return pair_int_hash(first, last);
}

constexpr unsigned ptr_hash(FlatPtr ptr)
{
    if constexpr (sizeof(ptr) == 8)
        return u64_hash(ptr);
    else
        return int_hash(ptr);
}

inline unsigned ptr_hash(void const* ptr)
{
    return ptr_hash(FlatPtr(ptr));
}
