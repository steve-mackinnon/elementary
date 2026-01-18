#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "../../Types.h"

namespace elem
{
    // Musical division values representing bar fractions
    enum class MusicalDivision : uint8_t {
        Div_1_256 = 0,
        Div_1_128,
        Div_1_64,
        Div_1_32,
        Div_1_16,
        Div_1_8,
        Div_1_4,
        Div_1_2,
        Div_1,
        Div_2,
        Div_4,
        Div_8,
        Div_16,
        Div_32,
    };

    // Bar.Beat.Subdivision notation (1-indexed)
    // e.g., "9.1.3" = 9th bar, 1st beat, 3rd sixteenth note
    struct BarBeatSubdivision {
        uint16_t bar;          // 1-indexed
        uint8_t beat;          // 1-indexed
        uint8_t subdivision;   // 1-indexed (sixteenth notes)
    };

    // Mask for encoding TimeValue into uint64_t
    // Layout: [61 bits payload][3 bits type tag]
    // For doubles, we store the raw bits with lower 3 bits replaced by type tag.
    // This loses ~3 bits of mantissa precision (negligible for time values).
    // 0x7 = 0b111, so ~0x7 = 0xFFFFFFFFFFFFFFF8 = all bits except lower 3
    constexpr uint64_t kTimeValuePayloadMask = ~0x7ULL;
    constexpr uint64_t kTimeValueTypeMask = 0x7ULL;

    // Time value type discriminant
    enum class TimeValueType : uint8_t {
        Invalid = 0,
        Seconds,
        Milliseconds,
        BarsNormal,
        BarsTriplet,
        BarsDotted,
        BarBeatSubdivision,
    };

    // Intermediate representation for parsed time values
    struct TimeValue {
        TimeValueType type;
        union {
            double timeSeconds;
            double timeMs;
            MusicalDivision division;
            BarBeatSubdivision barBeatSub;
        };

        TimeValue() : type(TimeValueType::Invalid), timeSeconds(0.0) {}
    };

    // Get the bar value for a musical division
    inline double getBarValue(MusicalDivision div) {
        switch (div) {
            case MusicalDivision::Div_1_256: return 1.0 / 256.0;
            case MusicalDivision::Div_1_128: return 1.0 / 128.0;
            case MusicalDivision::Div_1_64: return 1.0 / 64.0;
            case MusicalDivision::Div_1_32: return 1.0 / 32.0;
            case MusicalDivision::Div_1_16: return 1.0 / 16.0;
            case MusicalDivision::Div_1_8: return 1.0 / 8.0;
            case MusicalDivision::Div_1_4: return 1.0 / 4.0;
            case MusicalDivision::Div_1_2: return 1.0 / 2.0;
            case MusicalDivision::Div_1: return 1.0;
            case MusicalDivision::Div_2: return 2.0;
            case MusicalDivision::Div_4: return 4.0;
            case MusicalDivision::Div_8: return 8.0;
            case MusicalDivision::Div_16: return 16.0;
            case MusicalDivision::Div_32: return 32.0;
        }
    }

    // Pack TimeValue into uint64_t for atomic storage
    // Layout: [61 bits payload][3 bits type]
    // For doubles, we mask off lower 3 bits (negligible precision loss)
    inline uint64_t encodeTimeValue(TimeValue const& tv) {
        uint64_t typeTag = static_cast<uint64_t>(tv.type);

        switch (tv.type) {
            case TimeValueType::Seconds:
            case TimeValueType::Milliseconds: {
                // Store double as uint64_t bitwise, masking lower 3 bits for type
                uint64_t payload;
                std::memcpy(&payload, &tv.timeSeconds, sizeof(double));
                return typeTag | (payload & kTimeValuePayloadMask);
            }
            case TimeValueType::BarsNormal:
            case TimeValueType::BarsTriplet:
            case TimeValueType::BarsDotted: {
                // Store division enum value shifted left
                uint64_t payload = static_cast<uint64_t>(tv.division);
                return typeTag | (payload << 3);
            }
            case TimeValueType::BarBeatSubdivision: {
                // Pack bar (16 bits) + beat (8 bits) + subdivision (8 bits)
                uint64_t payload = (static_cast<uint64_t>(tv.barBeatSub.bar) << 16)
                                 | (static_cast<uint64_t>(tv.barBeatSub.beat) << 8)
                                 | static_cast<uint64_t>(tv.barBeatSub.subdivision);
                return typeTag | (payload << 3);
            }
            case TimeValueType::Invalid:
                return typeTag;
        }
    }

    // Unpack uint64_t back to TimeValue
    inline TimeValue decodeTimeValue(uint64_t encoded) {
        TimeValue tv;

        uint64_t typeTag = encoded & kTimeValueTypeMask;
        tv.type = static_cast<TimeValueType>(typeTag);

        switch (tv.type) {
            case TimeValueType::Seconds:
            case TimeValueType::Milliseconds: {
                uint64_t payload = encoded & kTimeValuePayloadMask;
                std::memcpy(&tv.timeSeconds, &payload, sizeof(double));
                break;
            }
            case TimeValueType::BarsNormal:
            case TimeValueType::BarsTriplet:
            case TimeValueType::BarsDotted: {
                uint64_t payload = encoded >> 3;
                tv.division = static_cast<MusicalDivision>(payload & 0xFF);
                break;
            }
            case TimeValueType::BarBeatSubdivision: {
                uint64_t payload = encoded >> 3;
                tv.barBeatSub.bar = static_cast<uint16_t>((payload >> 16) & 0xFFFF);
                tv.barBeatSub.beat = static_cast<uint8_t>((payload >> 8) & 0xFF);
                tv.barBeatSub.subdivision = static_cast<uint8_t>(payload & 0xFF);
                break;
            }
            case TimeValueType::Invalid:
                tv.timeSeconds = 0.0;
                break;
        }

        return tv;
    }

    namespace detail {

        // Parse seconds format: "2.5s"
        inline std::optional<TimeValue> parseSeconds(std::string const& str) {
            if (str.size() < 2 || str.back() != 's' || str[str.size() - 2] == 'm')
                return std::nullopt;

            try {
                TimeValue tv;
                tv.type = TimeValueType::Seconds;
                tv.timeSeconds = std::stod(str.substr(0, str.size() - 1));
                return tv;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Parse milliseconds format: "100ms"
        inline std::optional<TimeValue> parseMilliseconds(std::string const& str) {
            if (str.size() < 3 || str.substr(str.size() - 2) != "ms")
                return std::nullopt;

            try {
                TimeValue tv;
                tv.type = TimeValueType::Milliseconds;
                tv.timeMs = std::stod(str.substr(0, str.size() - 2));
                return tv;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Map denominator value to MusicalDivision for fractions (1/N)
        inline std::optional<MusicalDivision> fractionDenomToMusicalDivision(int denom) {
            switch (denom) {
                case 256: return MusicalDivision::Div_1_256;
                case 128: return MusicalDivision::Div_1_128;
                case 64: return MusicalDivision::Div_1_64;
                case 32: return MusicalDivision::Div_1_32;
                case 16: return MusicalDivision::Div_1_16;
                case 8: return MusicalDivision::Div_1_8;
                case 4: return MusicalDivision::Div_1_4;
                case 2: return MusicalDivision::Div_1_2;
                default: return std::nullopt;
            }
        }

        // Map whole bar count to MusicalDivision
        inline std::optional<MusicalDivision> wholeBarToMusicalDivision(int bars) {
            switch (bars) {
                case 1: return MusicalDivision::Div_1;
                case 2: return MusicalDivision::Div_2;
                case 4: return MusicalDivision::Div_4;
                case 8: return MusicalDivision::Div_8;
                case 16: return MusicalDivision::Div_16;
                case 32: return MusicalDivision::Div_32;
                default: return std::nullopt;
            }
        }

        struct ModifierSuffix {
            bool isTriplet;
            bool isDotted;
            std::string strippedString;
        };

        // Extract triplet/dotted suffix
        inline ModifierSuffix extractModifierSuffix(std::string const& str) {
            if (!str.empty() && str.back() == 't')
                return {true, false, str.substr(0, str.size() - 1)};
            if (!str.empty() && str.back() == 'd')
                return {false, true, str.substr(0, str.size() - 1)};
            return {false, false, str};
        }

        // Determine TimeValueType from triplet/dotted flags
        inline TimeValueType barTypeFromModifiers(bool isTriplet, bool isDotted) {
            if (isTriplet) return TimeValueType::BarsTriplet;
            if (isDotted) return TimeValueType::BarsDotted;
            return TimeValueType::BarsNormal;
        }

        // Parse bar fraction format: "1/4", "1/16t", "1/8d"
        inline std::optional<TimeValue> parseBarFraction(std::string const& str) {
            size_t slashPos = str.find('/');
            if (slashPos == std::string::npos)
                return std::nullopt;

            try {
                int numerator = std::stoi(str.substr(0, slashPos));
                if (numerator != 1)
                    return std::nullopt;

                auto [isTriplet, isDotted, denomPart] = extractModifierSuffix(str.substr(slashPos + 1));
                int denominator = std::stoi(denomPart);

                auto div = fractionDenomToMusicalDivision(denominator);
                if (!div)
                    return std::nullopt;

                TimeValue tv;
                tv.division = *div;
                tv.type = barTypeFromModifiers(isTriplet, isDotted);
                return tv;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Parse whole bar format: "1", "2", "4t", "8d"
        inline std::optional<TimeValue> parseWholeBar(std::string const& str) {
            try {
                auto [isTriplet, isDotted, numPart] = extractModifierSuffix(str);

                // Verify string is purely numeric (stoi would accept "2/4" as 2)
                if (numPart.empty() || !std::all_of(numPart.begin(), numPart.end(), ::isdigit))
                    return std::nullopt;

                int bars = std::stoi(numPart);

                auto div = wholeBarToMusicalDivision(bars);
                if (!div)
                    return std::nullopt;

                TimeValue tv;
                tv.division = *div;
                tv.type = barTypeFromModifiers(isTriplet, isDotted);
                return tv;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Parse bar.beat.subdivision format: "9.1.3" ONLY (1-indexed, all 3 required)
        // Subdivisions are sixteenth notes (1/16 of a beat)
        inline std::optional<TimeValue> parseBarBeatSubdivision(std::string const& str) {
            // Must contain only digits and dots, no other characters
            if (str.empty() || !std::all_of(str.begin(), str.end(),
                [](char c) { return std::isdigit(c) || c == '.'; }))
                return std::nullopt;

            // Cannot start or end with a dot
            if (str.front() == '.' || str.back() == '.')
                return std::nullopt;

            // Split by '.'
            std::vector<int> components;
            size_t start = 0;
            size_t end = 0;

            while (end != std::string::npos) {
                end = str.find('.', start);
                std::string part = str.substr(start, end == std::string::npos ? std::string::npos : end - start);

                if (part.empty())
                    return std::nullopt;

                try {
                    components.push_back(std::stoi(part));
                } catch (...) {
                    return std::nullopt;
                }

                start = end + 1;
            }

            // Must have exactly 3 components
            if (components.size() != 3)
                return std::nullopt;

            // Extract components
            uint16_t bar = components[0];
            uint8_t beat = components[1];
            uint8_t subdivision = components[2];

            // Validate: all must be >= 1 (1-indexed)
            if (bar < 1 || beat < 1 || subdivision < 1)
                return std::nullopt;

            // Validate ranges (reasonable limits)
            if (bar > 9999 || beat > 255 || subdivision > 255)
                return std::nullopt;

            TimeValue tv;
            tv.type = TimeValueType::BarBeatSubdivision;
            tv.barBeatSub.bar = bar;
            tv.barBeatSub.beat = beat;
            tv.barBeatSub.subdivision = subdivision;
            return tv;
        }

    } // namespace detail

    // Parse a string like "2.5s", "100ms", "1/4", "1/16t", "1/8d", "9.1.3"
    inline std::optional<TimeValue> parseTimeString(std::string const& str) {
        if (str.empty())
            return std::nullopt;

        if (auto r = detail::parseSeconds(str)) return r;
        if (auto r = detail::parseMilliseconds(str)) return r;
        if (auto r = detail::parseBarFraction(str)) return r;
        if (auto r = detail::parseBarBeatSubdivision(str)) return r;
        if (auto r = detail::parseWholeBar(str)) return r;

        return std::nullopt;
    }

    // Convert TimeValue to normalized position [0.0, 1.0]
    inline double toNormalizedPosition(
        TimeValue const& tv,
        double bpm,
        uint64_t bufferLengthSamples,
        double sampleRate,
        double timeSignatureNum = 4.0,
        double timeSignatureDenom = 4.0)
    {
        if (bufferLengthSamples == 0 || sampleRate <= 0.0) {
            return 0.0;
        }

        double timeInSeconds = 0.0;

        switch (tv.type) {
            case TimeValueType::Seconds:
                timeInSeconds = tv.timeSeconds;
                break;

            case TimeValueType::Milliseconds:
                timeInSeconds = tv.timeMs / 1000.0;
                break;

            case TimeValueType::BarsNormal:
            case TimeValueType::BarsTriplet:
            case TimeValueType::BarsDotted: {
                // Get base bar value
                double bars = getBarValue(tv.division);

                // Apply modifiers
                if (tv.type == TimeValueType::BarsTriplet) {
                    bars *= (2.0 / 3.0);
                } else if (tv.type == TimeValueType::BarsDotted) {
                    bars *= 1.5;
                }

                // Convert bars to beats
                double beats = bars * timeSignatureNum;

                // Convert beats to seconds using BPM
                if (bpm > 0.0) {
                    timeInSeconds = beats * 60.0 / bpm;
                }
                break;
            }

            case TimeValueType::BarBeatSubdivision: {
                // Convert bar.beat.subdivision to beats (1-indexed)
                // bar: number of complete bars (bar-1 complete bars + current bar)
                // beat: current beat within bar (beat-1)
                // subdivision: sixteenth notes within beat (subdivision-1) / 16.0
                double beatsFromBars = static_cast<double>(tv.barBeatSub.bar - 1) * timeSignatureNum;
                double beatsWithinBar = static_cast<double>(tv.barBeatSub.beat - 1);
                double beatsFromSubdivision = static_cast<double>(tv.barBeatSub.subdivision - 1) / 16.0;

                double totalBeats = beatsFromBars + beatsWithinBar + beatsFromSubdivision;

                // Convert beats to seconds using BPM
                if (bpm > 0.0) {
                    timeInSeconds = totalBeats * 60.0 / bpm;
                }
                break;
            }

            case TimeValueType::Invalid:
            default:
                return 0.0;
        }

        // Convert seconds to normalized position
        double bufferDurationSeconds = static_cast<double>(bufferLengthSamples) / sampleRate;
        double normalized = timeInSeconds / bufferDurationSeconds;

        // Note: Not clamped here to allow loop lengths > sample duration
        return normalized;
    }

    // Helper to decode and convert loop start/length time values to normalized positions
    // Returns pair of (loopStart, loopEnd) in normalized [0.0, 1.0] range
    inline std::pair<double, double> decodeAndConvertLoopPoints(
        uint64_t encodedStart,
        uint64_t encodedLength,
        uint64_t bufferLength,
        double sampleRate,
        CurrentTime const& currentTime)
    {
        // Decode time values
        auto loopStartTime = decodeTimeValue(encodedStart);
        auto loopLengthTime = decodeTimeValue(encodedLength);

        // Convert to normalized positions (fallback to defaults if invalid)
        auto const normalizedLoopStart = loopStartTime.type == TimeValueType::Invalid ? 0.0 :
            toNormalizedPosition(loopStartTime, currentTime.bpm, bufferLength, sampleRate,
                                 currentTime.timeSignatureNumerator,
                                 currentTime.timeSignatureDenominator);
        auto const normalizedLoopLength = loopLengthTime.type == TimeValueType::Invalid ? 1.0 :
            toNormalizedPosition(loopLengthTime, currentTime.bpm, bufferLength, sampleRate,
                                 currentTime.timeSignatureNumerator,
                                 currentTime.timeSignatureDenominator);

        // Compute end point from start + length
        auto const normalizedLoopEnd = normalizedLoopStart + normalizedLoopLength;

        return std::make_pair(normalizedLoopStart, normalizedLoopEnd);
    }

} // namespace elem
