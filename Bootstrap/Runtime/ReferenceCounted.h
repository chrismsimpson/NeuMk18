/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeReferenceCounted.h>
#else

#    include "Assertions.h"
#    include "Checked.h"
#    include "NonCopyable.h"
#    include "Platform.h"
#    include "StdLibExtras.h"

class ReferenceCountedBase {

    MAKE_NONCOPYABLE(ReferenceCountedBase);
    MAKE_NONMOVABLE(ReferenceCountedBase);

public:

    using RefCountType = unsigned int;

    ALWAYS_INLINE void ref() const {

        VERIFY(m_refCount > 0);
        
        VERIFY(!Checked<RefCountType>::additionWouldOverflow(m_refCount, 1));
        
        ++m_refCount;
    }

    [[nodiscard]] bool try_ref() const {

        if (m_refCount == 0) {

            return false;
        }

        ref();
        
        return true;
    }

    [[nodiscard]] RefCountType ref_count() const { return m_refCount; }

protected:

    ReferenceCountedBase() = default;

    ~ReferenceCountedBase() { VERIFY(!m_refCount); }

    ALWAYS_INLINE RefCountType deref_base() const {

        VERIFY(m_refCount);
        
        return --m_refCount;
    }

    RefCountType mutable m_refCount { 1 };
};

template<typename T>
class ReferenceCounted : public ReferenceCountedBase {

public:

    bool dereference() const {

        auto* that = const_cast<T*>(static_cast<T const*>(this));

        auto new_ref_count = deref_base();
        
        if (new_ref_count == 0) {
            
            if constexpr (requires { that->will_be_destroyed(); }) {

                that->will_be_destroyed();
            }

            delete static_cast<const T*>(this);
            
            return true;
        }

        return false;
    }
};

#endif
