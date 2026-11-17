// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QHash>

#include <chrono>
#include <optional>
#include <utility>

/**
 * Cache for data that is expensive to read and rarely changes (labels and firmware
 * limits of a sensor). An incomplete load (a limit that exists but could not be read)
 * is returned but not stored, so the next lookup loads again. The whole cache is
 * dropped by clear() and once it is older than its lifetime.
 */
template<typename Key, typename Value>
class RefreshingCache {
public:
    using Clock = std::chrono::steady_clock;

    /** What a loader returns: the value, and whether it may be cached. */
    struct Load {
        Value value;
        bool complete = true;
    };

    explicit RefreshingCache(const Clock::duration lifetime) : m_lifetime(lifetime) {}

    /** Drops everything, e.g. when the keys become invalid. */
    void clear() {
        m_entries.clear();
        m_filledSince.reset();
    }

    /** Drops everything when the oldest entry is older than the lifetime at @p now. */
    void expire(const Clock::time_point now) {
        if (m_filledSince && now - *m_filledSince >= m_lifetime)
            clear();
    }

    /** The cached value of @p key, or the result of @p loader (stored when complete). */
    template<typename Loader>
    Value get(const Key &key, Loader &&loader, const Clock::time_point now = Clock::now()) {
        if (const auto it = m_entries.constFind(key); it != m_entries.constEnd())
            return *it;
        Load load = std::forward<Loader>(loader)();
        if (load.complete) {
            if (!m_filledSince)
                m_filledSince = now;
            m_entries.insert(key, load.value);
        }
        return std::move(load.value);
    }

    [[nodiscard]] qsizetype size() const { return m_entries.size(); }

private:
    Clock::duration m_lifetime;
    QHash<Key, Value> m_entries;
    std::optional<Clock::time_point> m_filledSince;
};
