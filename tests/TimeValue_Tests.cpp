#include "../runtime/elem/third-party/choc/choc/platform/choc_Assert.h"
#include "../runtime/elem/third-party/choc/choc/platform/choc_UnitTest.h"
#include "../runtime/elem/builtins/helpers/TimeValue.h"

void testParseTimeString (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (ParseTimeString);

    {
        CHOC_TEST (ParseSeconds)
        auto result = elem::parseTimeString("2.5s");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::Seconds));
        CHOC_EXPECT_NEAR (result->timeSeconds, 2.5, 0.0001);
    }

    {
        CHOC_TEST (ParseSecondsInteger)
        auto result = elem::parseTimeString("10s");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::Seconds));
        CHOC_EXPECT_NEAR (result->timeSeconds, 10.0, 0.0001);
    }

    {
        CHOC_TEST (ParseMilliseconds)
        auto result = elem::parseTimeString("100ms");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::Milliseconds));
        CHOC_EXPECT_NEAR (result->timeMs, 100.0, 0.0001);
    }

    {
        CHOC_TEST (ParseMillisecondsDecimal)
        auto result = elem::parseTimeString("50.5ms");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::Milliseconds));
        CHOC_EXPECT_NEAR (result->timeMs, 50.5, 0.0001);
    }

    {
        CHOC_TEST (ParseBarFraction_1_4)
        auto result = elem::parseTimeString("1/4");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsNormal));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_1_4));
    }

    {
        CHOC_TEST (ParseBarFraction_1_16)
        auto result = elem::parseTimeString("1/16");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsNormal));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_1_16));
    }

    {
        CHOC_TEST (ParseBarFraction_1_8)
        auto result = elem::parseTimeString("1/8");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsNormal));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_1_8));
    }

    {
        CHOC_TEST (ParseBarFractionTriplet)
        auto result = elem::parseTimeString("1/16t");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsTriplet));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_1_16));
    }

    {
        CHOC_TEST (ParseBarFractionDotted)
        auto result = elem::parseTimeString("1/8d");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsDotted));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_1_8));
    }

    {
        CHOC_TEST (ParseWholeBars)
        auto result = elem::parseTimeString("4");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsNormal));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_4));
    }

    {
        CHOC_TEST (ParseWholeBarsTriplet)
        auto result = elem::parseTimeString("2t");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsTriplet));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_2));
    }

    {
        CHOC_TEST (ParseWholeBarsDotted)
        auto result = elem::parseTimeString("1d");
        CHOC_EXPECT_TRUE (result.has_value());
        CHOC_EXPECT_EQ (static_cast<int>(result->type), static_cast<int>(elem::TimeValueType::BarsDotted));
        CHOC_EXPECT_EQ (static_cast<int>(result->division), static_cast<int>(elem::MusicalDivision::Div_1));
    }

    {
        CHOC_TEST (ParseInvalidEmpty)
        auto result = elem::parseTimeString("");
        CHOC_EXPECT_FALSE (result.has_value());
    }

    {
        CHOC_TEST (ParseInvalidNonsense)
        auto result = elem::parseTimeString("abc");
        CHOC_EXPECT_FALSE (result.has_value());
    }

    {
        CHOC_TEST (ParseInvalidNumerator)
        auto result = elem::parseTimeString("2/4");
        CHOC_EXPECT_FALSE (result.has_value());
    }

    {
        CHOC_TEST (ParseInvalidDenominator)
        auto result = elem::parseTimeString("1/3");
        CHOC_EXPECT_FALSE (result.has_value());
    }
}

void testEncodeDecodeRoundtrip (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (EncodeDecodeRoundtrip);

    {
        CHOC_TEST (RoundtripSeconds)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Seconds;
        tv.timeSeconds = 3.14159;

        uint64_t encoded = elem::encodeTimeValue(tv);
        elem::TimeValue decoded = elem::decodeTimeValue(encoded);

        CHOC_EXPECT_EQ (static_cast<int>(decoded.type), static_cast<int>(elem::TimeValueType::Seconds));
        CHOC_EXPECT_NEAR (decoded.timeSeconds, 3.14159, 0.0001);
    }

    {
        CHOC_TEST (RoundtripMilliseconds)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Milliseconds;
        tv.timeMs = 250.5;

        uint64_t encoded = elem::encodeTimeValue(tv);
        elem::TimeValue decoded = elem::decodeTimeValue(encoded);

        CHOC_EXPECT_EQ (static_cast<int>(decoded.type), static_cast<int>(elem::TimeValueType::Milliseconds));
        CHOC_EXPECT_NEAR (decoded.timeMs, 250.5, 0.0001);
    }

    {
        CHOC_TEST (RoundtripBarsNormal)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::BarsNormal;
        tv.division = elem::MusicalDivision::Div_1_4;

        uint64_t encoded = elem::encodeTimeValue(tv);
        elem::TimeValue decoded = elem::decodeTimeValue(encoded);

        CHOC_EXPECT_EQ (static_cast<int>(decoded.type), static_cast<int>(elem::TimeValueType::BarsNormal));
        CHOC_EXPECT_EQ (static_cast<int>(decoded.division), static_cast<int>(elem::MusicalDivision::Div_1_4));
    }

    {
        CHOC_TEST (RoundtripBarsTriplet)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::BarsTriplet;
        tv.division = elem::MusicalDivision::Div_1_8;

        uint64_t encoded = elem::encodeTimeValue(tv);
        elem::TimeValue decoded = elem::decodeTimeValue(encoded);

        CHOC_EXPECT_EQ (static_cast<int>(decoded.type), static_cast<int>(elem::TimeValueType::BarsTriplet));
        CHOC_EXPECT_EQ (static_cast<int>(decoded.division), static_cast<int>(elem::MusicalDivision::Div_1_8));
    }

    {
        CHOC_TEST (RoundtripBarsDotted)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::BarsDotted;
        tv.division = elem::MusicalDivision::Div_1_16;

        uint64_t encoded = elem::encodeTimeValue(tv);
        elem::TimeValue decoded = elem::decodeTimeValue(encoded);

        CHOC_EXPECT_EQ (static_cast<int>(decoded.type), static_cast<int>(elem::TimeValueType::BarsDotted));
        CHOC_EXPECT_EQ (static_cast<int>(decoded.division), static_cast<int>(elem::MusicalDivision::Div_1_16));
    }

    {
        CHOC_TEST (RoundtripInvalid)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Invalid;

        uint64_t encoded = elem::encodeTimeValue(tv);
        elem::TimeValue decoded = elem::decodeTimeValue(encoded);

        CHOC_EXPECT_EQ (static_cast<int>(decoded.type), static_cast<int>(elem::TimeValueType::Invalid));
    }
}

void testToNormalizedPosition (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (ToNormalizedPosition);

    const double sampleRate = 44100.0;
    const uint64_t bufferLength = 44100; // 1 second buffer

    {
        CHOC_TEST (SecondsToNormalized)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Seconds;
        tv.timeSeconds = 0.5;

        double norm = elem::toNormalizedPosition(tv, 120.0, bufferLength, sampleRate);
        CHOC_EXPECT_NEAR (norm, 0.5, 0.0001);
    }

    {
        CHOC_TEST (MillisecondsToNormalized)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Milliseconds;
        tv.timeMs = 250.0;

        double norm = elem::toNormalizedPosition(tv, 120.0, bufferLength, sampleRate);
        CHOC_EXPECT_NEAR (norm, 0.25, 0.0001);
    }

    {
        CHOC_TEST (ClampToMax)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Seconds;
        tv.timeSeconds = 2.0; // Beyond buffer length

        double norm = elem::toNormalizedPosition(tv, 120.0, bufferLength, sampleRate);
        CHOC_EXPECT_NEAR (norm, 1.0, 0.0001);
    }

    {
        CHOC_TEST (MusicalTimeAtBpm120)
        // At 120 BPM, 4/4, one bar = 2 seconds
        // 1/4 note = 0.5 bars = 1 beat = 0.5 seconds at 120 BPM
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::BarsNormal;
        tv.division = elem::MusicalDivision::Div_1_4;

        double norm = elem::toNormalizedPosition(tv, 120.0, bufferLength, sampleRate, 4.0, 4.0);
        // 1/4 bar * 4 beats/bar = 1 beat, at 120 BPM = 0.5s, normalized = 0.5
        CHOC_EXPECT_NEAR (norm, 0.5, 0.0001);
    }

    {
        CHOC_TEST (MusicalTimeOneBar)
        // At 120 BPM, 4/4, one bar = 2 seconds
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::BarsNormal;
        tv.division = elem::MusicalDivision::Div_1;

        // For a 2-second buffer
        uint64_t twoSecBuffer = 88200;
        double norm = elem::toNormalizedPosition(tv, 120.0, twoSecBuffer, sampleRate, 4.0, 4.0);
        CHOC_EXPECT_NEAR (norm, 1.0, 0.0001);
    }

    {
        CHOC_TEST (ZeroBufferLength)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Seconds;
        tv.timeSeconds = 0.5;

        double norm = elem::toNormalizedPosition(tv, 120.0, 0, sampleRate);
        CHOC_EXPECT_NEAR (norm, 0.0, 0.0001);
    }

    {
        CHOC_TEST (InvalidTimeValue)
        elem::TimeValue tv;
        tv.type = elem::TimeValueType::Invalid;

        double norm = elem::toNormalizedPosition(tv, 120.0, bufferLength, sampleRate);
        CHOC_EXPECT_NEAR (norm, 0.0, 0.0001);
    }
}

void testGetBarValue (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (GetBarValue);

    {
        CHOC_TEST (Div_1_4)
        double val = elem::getBarValue(elem::MusicalDivision::Div_1_4);
        CHOC_EXPECT_NEAR (val, 0.25, 0.0001);
    }

    {
        CHOC_TEST (Div_1_8)
        double val = elem::getBarValue(elem::MusicalDivision::Div_1_8);
        CHOC_EXPECT_NEAR (val, 0.125, 0.0001);
    }

    {
        CHOC_TEST (Div_1_16)
        double val = elem::getBarValue(elem::MusicalDivision::Div_1_16);
        CHOC_EXPECT_NEAR (val, 0.0625, 0.0001);
    }

    {
        CHOC_TEST (Div_1)
        double val = elem::getBarValue(elem::MusicalDivision::Div_1);
        CHOC_EXPECT_NEAR (val, 1.0, 0.0001);
    }

    {
        CHOC_TEST (Div_2)
        double val = elem::getBarValue(elem::MusicalDivision::Div_2);
        CHOC_EXPECT_NEAR (val, 2.0, 0.0001);
    }
}

int main()
{
    choc::test::TestProgress progress;

    testParseTimeString(progress);
    testEncodeDecodeRoundtrip(progress);
    testToNormalizedPosition(progress);
    testGetBarValue(progress);

    progress.printReport();

    return progress.numFails == 0 ? 0 : 1;
}
