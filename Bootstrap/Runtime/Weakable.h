/*
 * Copyright (c) 2018-2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef KERNEL
#    include <Kernel/Library/ThreadSafeWeakable.h>
#else
#    include "Assertions.h"
#    include "Atomic.h"
#    include "ReferenceCounted.h"
#    include "ReferencePointer.h"
#    include "StdLibExtras.h"

template<typename T>
class Weakable;

template<typename T>
class WeakPointer;

class WeakLink : public ReferenceCounted<WeakLink> {
    
    template<typename T>
    friend class Weakable;
    
    template<typename T>
    friend class WeakPointer;

public:

    template<typename T>
    ReferencePointer<T> strong_ref() const requires(IsBaseOf<ReferenceCountedBase, T>) {
            
        ReferencePointer<T> ref;

        {
            if (!(m_consumers.fetchAdd(1u << 1, MemoryOrder::memory_order_acquire) & 1u)) {

                T* ptr = (T*)m_pointer.load(MemoryOrder::memory_order_acquire);
                
                if (ptr && ptr->try_ref()) {

                    ref = adopt_ref(*ptr);
                }
            }

            m_consumers.fetchSub(1u << 1, MemoryOrder::memory_order_release);
        }

        return ref;
    }

    template<typename T>
    T* unsafePointer() const {

        if (m_consumers.load(MemoryOrder::memory_order_relaxed) & 1u) {

            return nullptr;
        }

        // NOTE: This may return a non-null pointer even if revocation
        // has been triggered as there is a possible race! But it's "unsafe"
        // anyway because we return a raw pointer without ensuring a
        // reference...
        
        return (T*)m_pointer.load(MemoryOrder::memory_order_acquire);
    }

    bool isNull() const {

        return unsafePointer<void>() == nullptr;
    }

    void revoke() {

        auto current_consumers = m_consumers.fetchOr(1u, MemoryOrder::memory_order_relaxed);
        
        VERIFY(!(current_consumers & 1u));
        
        // We flagged revocation, now wait until everyone trying to obtain
        // a strong reference is done
        
        while (current_consumers > 0) {
            
            current_consumers = m_consumers.load(MemoryOrder::memory_order_acquire) & ~1u;
        }
        
        // No one is trying to use it (anymore)
        
        m_pointer.store(nullptr, MemoryOrder::memory_order_release);
    }

private:

    template<typename T>
    explicit WeakLink(T& weakable)
        : m_pointer(&weakable) { }

    mutable Atomic<void*> m_pointer;
    mutable Atomic<unsigned> m_consumers; // LSB indicates revocation in progress
};

///

template<typename T>
class Weakable {

private:

    class Link;

public:

    template<typename U = T>
    WeakPointer<U> makeWeakPointer() const {
        
        return MUST(tryMakeWeakPointer<U>());
    }

    template<typename U = T>
    ErrorOr<WeakPointer<U>> tryMakeWeakPointer() const;

protected:
    Weakable() = default;

    ~Weakable() {

        revoke_weak_ptrs();
    }

    void revoke_weak_ptrs() {

        if (auto link = move(m_link)) {

            link->revoke();
        }
    }

private:

    mutable ReferencePointer<WeakLink> m_link;
};

#endif
