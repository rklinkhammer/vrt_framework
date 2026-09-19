#include <vita/codec/numeric_samples.hpp>
#include <vita/codec/general_samples.hpp>
#include <array>
#include <cassert>
#include <cstdlib>
#include <new>
#include <type_traits>
using namespace vita::codec::numeric;
static std::size_t allocations = 0;
void* operator new(std::size_t n) { ++allocations; if (auto p = std::malloc(n)) return p; std::abort(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
constexpr ConversionPolicy exact{Precision::exact_only, Rounding::nearest_ties_even, Overflow::error, VrtEncoding::lowest_exponent};
constexpr ConversionPolicy rounded(Rounding r, Overflow o = Overflow::error) {
    return {Precision::allow_rounding, r, o, VrtEncoding::lowest_exponent};
}
static BinaryValue value(std::uint64_t mag, int e, bool neg = false) {
    return {mag, static_cast<std::int16_t>(e), neg};
}
static void anchors() {
    const NumericSpec u{Kind::unsigned_vrt, 5, 0, 2}, s{Kind::signed_vrt, 5, 0, 2};
    assert(*decode_exact(u, 31) == value(7, -3));
    assert(*decode_exact(u, 4) == value(1, -6));
    assert(*decode_exact(u, 16) == value(1, -4)); // normative formula, Table D-1 typo
    assert(*decode_exact(s, 31) == value(1, -2, true));
    assert(*decode_exact(s, 28) == value(1, -5, true));
    assert(*decode_exact(s, 19) == value(1, 0, true));
    assert(*decode_exact(s, 15) == value(3, -2));
    assert(encode_numeric(u, value(1, -3), exact)->bits == 17); // 4 * 2^-5, lowest exponent
    assert(encode_numeric(u, value(0, 32767, true), exact)->bits == 0);
    assert(*decode_exact({Kind::unsigned_non_normalized, 64}, UINT64_MAX) == value(UINT64_MAX, 0));
    assert(*decode_exact({Kind::signed_non_normalized, 64}, 1ULL << 63) == value(1, 63, true));
    assert(encode_numeric({Kind::signed_non_normalized, 64}, value(1, 63, true), exact)->bits == (1ULL << 63));
    assert(encode_numeric({Kind::unsigned_non_normalized, 64}, value(UINT64_MAX, 0), exact)->bits == UINT64_MAX);
    assert(*decode_exact({Kind::signed_fixed, 1}, 1) == value(1, 0, true));
    assert(*decode_exact({Kind::unsigned_fixed, 1}, 1) == value(1, -1));
    for (unsigned e = 1; e <= 6; ++e) {
        for (auto k : {Kind::unsigned_vrt, Kind::signed_vrt}) {
            NumericSpec spec{k, 64, 0, e};
            for (auto bits : {std::uint64_t{0}, std::uint64_t{1} << e, UINT64_MAX, std::uint64_t{1} << 63}) {
                auto decoded = decode_exact(spec, bits); assert(decoded);
                auto encoded = encode_numeric(spec, *decoded, exact); assert(encoded);
                assert(*decode_exact(spec, encoded->bits) == *decoded);
            }
        }
    }
    for (unsigned n = 1; n <= 64; ++n) {
        const auto mask = n == 64 ? UINT64_MAX : (std::uint64_t{1} << n) - 1;
        for (auto kind : {Kind::unsigned_fixed, Kind::signed_fixed,
                          Kind::unsigned_non_normalized, Kind::signed_non_normalized}) {
            NumericSpec spec{kind, n};
            for (auto bits : {std::uint64_t{0}, std::uint64_t{1}, mask, std::uint64_t{1} << (n - 1)}) {
                auto decoded = decode_exact(spec, bits); assert(decoded);
                auto encoded = encode_numeric(spec, *decoded, exact);
                assert(encoded && encoded->bits == bits && !encoded->rounded && !encoded->saturated);
            }
        }
    }
    auto negative_unsigned = encode_numeric({Kind::unsigned_fixed, 64}, value(1, -2, true),
                                           rounded(Rounding::toward_positive, Overflow::saturate));
    assert(negative_unsigned && negative_unsigned->bits == 0 && negative_unsigned->saturated);
    const ConversionPolicy exact_saturate{Precision::exact_only, Rounding::toward_zero,
                                         Overflow::saturate, VrtEncoding::lowest_exponent};
    assert(!encode_numeric({Kind::unsigned_fixed, 8}, value(1, 0), exact_saturate));
    auto negative_clip = encode_numeric({Kind::signed_non_normalized, 64}, value(1, 64, true),
                                        rounded(Rounding::toward_zero, Overflow::saturate));
    assert(negative_clip && negative_clip->bits == (1ULL << 63) && negative_clip->saturated);
    NumericSpec integer{Kind::signed_non_normalized, 8};
    assert(encode_numeric(integer, value(5, -1), rounded(Rounding::nearest_ties_even))->bits == 2);
    assert(encode_numeric(integer, value(7, -1), rounded(Rounding::nearest_ties_even))->bits == 4);
    assert(encode_numeric(integer, value(5, -1, true), rounded(Rounding::toward_negative))->bits == 253);
    assert(encode_numeric(integer, value(5, -1, true), rounded(Rounding::toward_positive))->bits == 254);
    assert(!encode_numeric(integer, value(5, -1), exact));
    auto clipped = encode_numeric(integer, value(1, 32767), rounded(Rounding::toward_zero, Overflow::saturate));
    assert(clipped && clipped->bits == 127 && clipped->saturated && !clipped->rounded);
    assert(!encode_numeric(integer, value(1, 32767), exact));
    assert(!encode_numeric(integer, value(255, -1), rounded(Rounding::toward_zero))); // domain first
    assert(encode_numeric(integer, value(1, -32768), rounded(Rounding::toward_positive))->bits == 1);
    assert(encode_numeric(integer, value(1, -32768), rounded(Rounding::nearest_ties_even))->bits == 0);
    assert(encode_numeric({Kind::unsigned_non_normalized, 64}, value(1ULL << 63, -64), rounded(Rounding::nearest_ties_even))->bits == 0);
    assert(encode_numeric({Kind::unsigned_non_normalized, 64}, value((1ULL << 63) + 1, -64), rounded(Rounding::nearest_ties_even))->bits == 1);
    assert(!canonicalize(value(2, 32767)));
    assert(*canonicalize(value(0, 32767, true)) == value(0, 0));
    assert(!decode_exact({Kind::unsigned_fixed, 8}, 256));
    assert(!decode_exact({Kind::unsigned_fixed, 0}, 0));
    assert(!decode_exact({Kind::unsigned_non_normalized, 64, 16}, 0));
    assert(!decode_exact({Kind::unsigned_vrt, 6, 0, 6}, 0));
    assert(!decode_exact({Kind::unsigned_fixed, 8, 1}, 0));
    assert(!decode_exact({static_cast<Kind>(99), 8}, 0));
    auto bad = exact; bad.rounding = static_cast<Rounding>(99);
    assert(!encode_numeric(integer, value(0, 0), bad));
    auto converted = convert_numeric({Kind::unsigned_fixed, 8}, 128, {Kind::unsigned_vrt, 5, 0, 2}, exact);
    assert(converted && *decode_exact(u, converted->bits) == value(1, -1));
}
// Independent small rational oracle: all representable values are signed integers
// on an explicit shared grid. Search the full finite set, not the production quantizer.
static void exhaustive(NumericSpec spec, bool sign, unsigned fraction, unsigned ebits) {
    const unsigned m = spec.bits - ebits;
    const unsigned maxe = ebits ? (1u << ebits) - 1 : 0;
    const int base = -static_cast<int>(fraction + maxe) - 2;
    std::array<std::int64_t, 256> represented{};
    const unsigned count = 1u << spec.bits;
    for (unsigned raw = 0; raw < count; ++raw) {
        const unsigned e = ebits ? raw % (1u << ebits) : 0;
        const unsigned mant = raw >> ebits;
        const int integer = sign && mant >= (1u << (m - 1)) ? static_cast<int>(mant) - (1 << m) : static_cast<int>(mant);
        represented[raw] = static_cast<std::int64_t>(integer) * (std::int64_t{1} << (e + 2));
        auto decoded = decode_exact(spec, raw); assert(decoded);
        auto d = *decoded;
        const auto actual = static_cast<std::int64_t>(d.magnitude) * (std::int64_t{1} << (d.exponent - base)) * (d.negative ? -1 : 1);
        assert(actual == represented[raw]);
    }
    std::int64_t lo = represented[0], hi = represented[0];
    for (unsigned raw = 0; raw < count; ++raw) { if (represented[raw] < lo) lo = represented[raw]; if (represented[raw] > hi) hi = represented[raw]; }
    for (std::int64_t input = lo - 2; input <= hi + 2; ++input) {
        const auto binary = value(static_cast<std::uint64_t>(input < 0 ? -input : input), base, input < 0);
        for (auto rounding : {Rounding::nearest_ties_even, Rounding::toward_zero, Rounding::toward_negative, Rounding::toward_positive}) {
            auto result = encode_numeric(spec, binary, rounded(rounding));
            if (input < lo || input > hi) { assert(!result); continue; }
            assert(result);
            std::int64_t lower = lo, upper = hi;
            for (unsigned raw = 0; raw < count; ++raw) {
                const auto v = represented[raw];
                if (v <= input && v > lower) lower = v;
                if (v >= input && v < upper) upper = v;
            }
            std::int64_t expected = lower;
            if (rounding == Rounding::toward_positive || (rounding == Rounding::toward_zero && input < 0)) expected = upper;
            if (rounding == Rounding::nearest_ties_even) {
                if (upper - input < input - lower) expected = upper;
                else if (upper != lower && upper - input == input - lower && (lower / (upper - lower)) % 2) expected = upper;
            }
            assert(represented[result->bits] == expected);
            assert(result->rounded == (expected != input));
            assert(!result->saturated);
            unsigned canonical = 0, best_e = 100;
            for (unsigned raw = 0; raw < count; ++raw) {
                const unsigned e = ebits ? raw % (1u << ebits) : 0;
                if (represented[raw] == expected && e < best_e) { canonical = raw; best_e = e; }
            }
            assert(result->bits == canonical);
            auto precise = encode_numeric(spec, binary, exact);
            assert(static_cast<bool>(precise) == (lower == upper));
        }
    }
}
static void raw_integration() {
    using namespace vita::codec::general;
    PackingSpec packing{}; packing.item_bits = 8; packing.packing_bits = 12;
    packing.channel_tag_bits = 2; packing.event_tag_bits = 2;
    std::array<Item, 2> items{{{128, 2, 1}, {64, 3, 2}}};
    for (auto& item : items) {
        auto result = convert_numeric({Kind::unsigned_fixed, 8}, item.data_bits,
                                      {Kind::unsigned_non_normalized, 8, 2}, exact);
        assert(result); item.data_bits = result->bits;
    }
    std::array<std::byte, 4> bytes{};
    assert(pack(packing, 2, items, bytes));
    auto view = PackedSamples::create(bytes, packing, 2); assert(view);
    assert(*view->at(0) == (Item{2, 2, 1})); assert(*view->at(1) == (Item{1, 3, 2}));
}
int main() {
    static_assert(sizeof(BinaryValue) == 16);
    static_assert(!std::is_default_constructible_v<ConversionPolicy>);
    const auto before = allocations;
    anchors();
    for (unsigned n = 1; n <= 6; ++n) {
        for (bool sign : {false, true}) {
            exhaustive({sign ? Kind::signed_fixed : Kind::unsigned_fixed, n}, sign, n - static_cast<unsigned>(sign), 0);
            for (unsigned f = 0; f < n; ++f)
                exhaustive({sign ? Kind::signed_non_normalized : Kind::unsigned_non_normalized, n, f}, sign, f, 0);
            for (unsigned e = 1; e <= 3 && e < n; ++e)
                exhaustive({sign ? Kind::signed_vrt : Kind::unsigned_vrt, n, 0, e}, sign, n - e - static_cast<unsigned>(sign), e);
        }
    }
    raw_integration();
    assert(allocations == before);
}
