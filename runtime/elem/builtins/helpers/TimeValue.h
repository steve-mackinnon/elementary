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

    // Time value type discriminant
    enum class TimeValueType : uint8_t {
        Invalid = 0,
        Seconds,
        Milliseconds,
        BarsNormal,
        BarsTriplet,
        BarsDotted,
    };

    // Intermediate representation for parsed time values
    struct TimeValue {
        TimeValueType type;
        union {
            double timeSeconds;
            double timeMs;
            MusicalDivision division;
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
            default: return 1.0;
        }
    }

    // Pack TimeValue into uint64_t for atomic storage
    // Layout: [3 bits type][61 bits payload]
    inline uint64_t encodeTimeValue(TimeValue const& tv) {
        uint64_t encoded = 0;
        uint64_t typeTag = static_cast<uint64_t>(tv.type);

        encoded = typeTag;

        switch (tv.type) {
            case TimeValueType::Seconds:
            case TimeValueType::Milliseconds: {
                // Store double as uint64_t bitwise
                uint64_t payload;
                std::memcpy(&payload, &tv.timeSeconds, sizeof(double));
                encoded |= (payload << 3);
                break;
            }
            case TimeValueType::BarsNormal:
            case TimeValueType::BarsTriplet:
            case TimeValueType::BarsDotted: {
                // Store division enum value
                uint64_t payload = static_cast<uint64_t>(tv.division);
                encoded |= (payload << 3);
                break;
            }
            case TimeValueType::Invalid:
            default:
                break;
        }

        return encoded;
    }

    // Unpack uint64_t back to TimeValue
    inline TimeValue decodeTimeValue(uint64_t encoded) {
        TimeValue tv;

        uint64_t typeTag = encoded & 0x7; // Lower 3 bits
        tv.type = static_cast<TimeValueType>(typeTag);

        uint64_t payload = encoded >> 3;

        switch (tv.type) {
            case TimeValueType::Seconds:
            case TimeValueType::Milliseconds: {
                std::memcpy(&tv.timeSeconds, &payload, sizeof(double));
                break;
            }
            case TimeValueType::BarsNormal:
            case TimeValueType::BarsTriplet:
            case TimeValueType::BarsDotted: {
                tv.division = static_cast<MusicalDivision>(payload & 0xFF);
                break;
            }
            case TimeValueType::Invalid:
            default:
                tv.timeSeconds = 0.0;
                break;
        }

        return tv;
    }

    // Parse a string like "2.5s", "100ms", "1/4", "1/16t", "1/8d"
    inline std::optional<TimeValue> parseTimeString(std::string const& str) {
        if (str.empty()) {
            return std::nullopt;
        }

        TimeValue tv;

        // Check for seconds suffix
        if (str.size() >= 2 && str.back() == 's' && str[str.size() - 2] != 'm') {
            try {
                tv.type = TimeValueType::Seconds;
                tv.timeSeconds = std::stod(str.substr(0, str.size() - 1));
                return tv;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Check for milliseconds suffix
        if (str.size() >= 3 && str.substr(str.size() - 2) == "ms") {
            try {
                tv.type = TimeValueType::Milliseconds;
                tv.timeMs = std::stod(str.substr(0, str.size() - 2));
                return tv;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Check for bar fraction pattern: "numerator/denominator[t|d]"
        size_t slashPos = str.find('/');
        if (slashPos != std::string::npos) {
            try {
                // Parse numerator and denominator
                int numerator = std::stoi(str.substr(0, slashPos));

                // Check for triplet or dotted suffix
                bool isTriplet = false;
                bool isDotted = false;
                std::string denomPart = str.substr(slashPos + 1);

                if (!denomPart.empty() && denomPart.back() == 't') {
                    isTriplet = true;
                    denomPart = denomPart.substr(0, denomPart.size() - 1);
                } else if (!denomPart.empty() && denomPart.back() == 'd') {
                    isDotted = true;
                    denomPart = denomPart.substr(0, denomPart.size() - 1);
                }

                int denominator = std::stoi(denomPart);

                // Map denominator to MusicalDivision (only support standard power-of-2 values)
                // Only numerator of 1 is supported for now
                if (numerator != 1) {
                    return std::nullopt;
                }

                MusicalDivision div;
                switch (denominator) {
                    case 256: div = MusicalDivision::Div_1_256; break;
                    case 128: div = MusicalDivision::Div_1_128; break;
                    case 64: div = MusicalDivision::Div_1_64; break;
                    case 32: div = MusicalDivision::Div_1_32; break;
                    case 16: div = MusicalDivision::Div_1_16; break;
                    case 8: div = MusicalDivision::Div_1_8; break;
                    case 4: div = MusicalDivision::Div_1_4; break;
                    case 2: div = MusicalDivision::Div_1_2; break;
                    default: return std::nullopt;
                }

                tv.division = div;
                tv.type = isTriplet ? TimeValueType::BarsTriplet :
                          isDotted ? TimeValueType::BarsDotted :
                          TimeValueType::BarsNormal;

                return tv;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Check for whole number bars: "1", "2", "4", etc (with optional t/d suffix)
        bool isTriplet = false;
        bool isDotted = false;
        std::string numPart = str;

        if (!str.empty() && str.back() == 't') {
            isTriplet = true;
            numPart = str.substr(0, str.size() - 1);
        } else if (!str.empty() && str.back() == 'd') {
            isDotted = true;
            numPart = str.substr(0, str.size() - 1);
        }

        try {
            int bars = std::stoi(numPart);

            MusicalDivision div;
            switch (bars) {
                case 1: div = MusicalDivision::Div_1; break;
                case 2: div = MusicalDivision::Div_2; break;
                case 4: div = MusicalDivision::Div_4; break;
                case 8: div = MusicalDivision::Div_8; break;
                case 16: div = MusicalDivision::Div_16; break;
                case 32: div = MusicalDivision::Div_32; break;
                default: return std::nullopt;
            }

            tv.division = div;
            tv.type = isTriplet ? TimeValueType::BarsTriplet :
                      isDotted ? TimeValueType::BarsDotted :
                      TimeValueType::BarsNormal;

            return tv;
        } catch (...) {
            return std::nullopt;
        }

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

            case TimeValueType::Invalid:
            default:
                return 0.0;
        }

        // Convert seconds to normalized position
        double bufferDurationSeconds = static_cast<double>(bufferLengthSamples) / sampleRate;
        double normalized = timeInSeconds / bufferDurationSeconds;

        // Clamp to [0.0, 1.0]
        return std::max(0.0, std::min(1.0, normalized));
    }

    // Helper to decode and convert loop start/end time values to normalized positions
    // Returns pair of (loopStart, loopEnd) in normalized [0.0, 1.0] range
    inline std::pair<double, double> decodeAndConvertLoopPoints(
        uint64_t encodedStart,
        uint64_t encodedEnd,
        uint64_t bufferLength,
        double sampleRate,
        CurrentTime const& currentTime)
    {
        // Decode time values
        auto lstartTV = decodeTimeValue(encodedStart);
        auto lendTV = decodeTimeValue(encodedEnd);

        // Convert to normalized positions (fallback to defaults if invalid)
        auto const lstart = lstartTV.type == TimeValueType::Invalid ? 0.0 :
            toNormalizedPosition(lstartTV, currentTime.bpm, bufferLength, sampleRate,
                                 currentTime.timeSignatureNumerator,
                                 currentTime.timeSignatureDenominator);
        auto const lend = lendTV.type == TimeValueType::Invalid ? 1.0 :
            toNormalizedPosition(lendTV, currentTime.bpm, bufferLength, sampleRate,
                                 currentTime.timeSignatureNumerator,
                                 currentTime.timeSignatureDenominator);

        return std::make_pair(lstart, lend);
    }

} // namespace elem
