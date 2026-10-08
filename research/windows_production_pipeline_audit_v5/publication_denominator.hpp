#pragma once

// LatestFrame has three leased buffers. Under stress it may reject an incoming
// frame, and close() may race the final synthetic-result callback. Neither is
// a successful publication. Account for both without changing the normal-cost
// zero-drop predicate or subtracting drops from the requested denominator.
std::size_t verify_publication_denominator(const J& summary, const J& attempts,
                                          std::size_t requested, std::size_t produced,
                                          Ns round_stop) {
    w::require(requested >= 1000 && requested <= 10000 && produced >= requested && produced <= requested + 64 &&
               attempts.is_array() && attempts.size() == requested + 64 && round_stop > 0,
               "bounded requested producer denominator");
    const auto count = [&](const J& value) {
        w::require(value.is_number_unsigned(), "unsigned publication counter");
        const auto n = value.get<std::uint64_t>();
        w::require(n <= produced, "publication counter bound");
        return static_cast<std::size_t>(n);
    };
    const auto published = count(summary.at("published"));
    const auto dropped = count(summary.at("pool_drops"));
    w::require(count(summary.at("candidate_packet_counters").at("physical_pool_drops")) == dropped,
               "same physical pool drop counter");
    w::require(published > 0 && dropped <= produced - published,
               "producer publication/drop denominator");
    const auto tail = produced - published - dropped;
    w::require(tail <= 1, "unaccounted producer attempts");
    if (tail) {
        const auto& last = attempts.at(produced - 1);
        w::require(last.at("attempt").get<std::size_t>() > requested &&
                   last.at("produced").get<bool>() &&
                   last.at("render_start_ns").get<Ns>() >= round_stop,
                   "unpublished attempt must follow synthetic result stop");
    }
    return tail;
}
