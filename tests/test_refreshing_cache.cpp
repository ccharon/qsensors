// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/refreshing_cache.h"

#include <QString>
#include <QtTest/QtTest>

#include <chrono>

// Verifies the cache LibsensorsSource keeps labels and firmware limits in.
class RefreshingCacheTest final : public QObject {
    Q_OBJECT

private slots:
    void complete_load_is_read_once();
    void incomplete_load_is_retried_until_complete();
    void clear_reloads();
    void lifetime_expires_the_cache();
};

using namespace std::chrono_literals;

namespace {
    using Cache = RefreshingCache<int, QString>;

    /** Loader that counts its calls and reports @p complete. */
    struct CountingLoader {
        int *calls;
        bool complete = true;

        Cache::Load operator()() const {
            ++*calls;
            return {.value = QStringLiteral("load %1").arg(*calls), .complete = complete};
        }
    };
}

void RefreshingCacheTest::complete_load_is_read_once() {
    Cache cache(1min);
    int calls = 0;
    QCOMPARE(cache.get(1, CountingLoader{&calls}), QStringLiteral("load 1"));
    QCOMPARE(cache.get(1, CountingLoader{&calls}), QStringLiteral("load 1"));
    QCOMPARE(calls, 1);
    // Another key is loaded on its own.
    QCOMPARE(cache.get(2, CountingLoader{&calls}), QStringLiteral("load 2"));
    QCOMPARE(calls, 2);
}

void RefreshingCacheTest::incomplete_load_is_retried_until_complete() {
    Cache cache(1min);
    int calls = 0;
    // A limit could not be read: this value is used, but the next lookup reads again.
    QCOMPARE(cache.get(1, CountingLoader{&calls, false}), QStringLiteral("load 1"));
    QCOMPARE(cache.size(), 0);
    QCOMPARE(cache.get(1, CountingLoader{&calls, false}), QStringLiteral("load 2"));
    QCOMPARE(cache.get(1, CountingLoader{&calls, true}), QStringLiteral("load 3"));
    QCOMPARE(cache.get(1, CountingLoader{&calls, true}), QStringLiteral("load 3"));
    QCOMPARE(calls, 3);
}

void RefreshingCacheTest::clear_reloads() {
    Cache cache(1min);
    int calls = 0;
    (void) cache.get(1, CountingLoader{&calls});
    cache.clear();
    QCOMPARE(cache.get(1, CountingLoader{&calls}), QStringLiteral("load 2"));
}

void RefreshingCacheTest::lifetime_expires_the_cache() {
    Cache cache(1min);
    const Cache::Clock::time_point start{};
    int calls = 0;
    (void) cache.get(1, CountingLoader{&calls}, start);
    (void) cache.get(2, CountingLoader{&calls}, start + 50s);

    cache.expire(start + 59s);
    QCOMPARE(cache.size(), 2);
    // The age counts from the first entry, so all entries are refreshed together.
    cache.expire(start + 60s);
    QCOMPARE(cache.size(), 0);
    QCOMPARE(cache.get(1, CountingLoader{&calls}, start + 61s), QStringLiteral("load 3"));

    // The next lifetime starts with the next stored entry.
    cache.expire(start + 61s + 59s);
    QCOMPARE(cache.size(), 1);
}

QTEST_APPLESS_MAIN(RefreshingCacheTest)
#include "test_refreshing_cache.moc"
