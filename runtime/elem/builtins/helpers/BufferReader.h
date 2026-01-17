#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>

#include "GainFade.h"
#include "elem/SharedResource.h"

namespace elem
{
    template <typename FloatType>
    class BufferReader {
    public:
        BufferReader(double sampleRate_, double fadeTime)
            : fade(sampleRate_, fadeTime, fadeTime)
            , loopStartFade(sampleRate_, fadeTime, fadeTime)
            , sampleRate(sampleRate_)
            , fadeTimeMs(fadeTime)
        {}

        void engage (double _position) {
            fade.fadeIn();
            position = _position;
            inLoopCrossfade = false;
            loopStartFade.reset();
       }

        void disengage() {
            fade.fadeOut();
            inLoopCrossfade = false;
            loopStartFade.reset();
        }

        template <typename DestType>
        struct ReadContext {
            ReadContext(
                SharedResource* source,
                DestType** outputData,
                size_t numChannels,
                size_t numSamples,
                std::optional<uint64_t> startOffsetSamples = std::nullopt,
                std::optional<uint64_t> stopOffsetSamples = std::nullopt,
                std::optional<std::pair<double, double>> loopRange = std::nullopt,
                bool shouldLoop = false,
                double playbackRate = 1.0,
                size_t writeOffset = 0
            )
                : source(source)
                , outputData(outputData)
                , numChannels(numChannels)
                , numSamples(numSamples)
                , startOffsetSamples(startOffsetSamples)
                , stopOffsetSamples(stopOffsetSamples)
                , loopRange(loopRange)
                , shouldLoop(shouldLoop)
                , playbackRate(playbackRate)
                , writeOffset(writeOffset)
            {}

            SharedResource* source;
            DestType** outputData;
            size_t numChannels;
            size_t numSamples;
            std::optional<uint64_t> startOffsetSamples;
            std::optional<uint64_t> stopOffsetSamples;
            std::optional<std::pair<double, double>> loopRange;
            bool shouldLoop;
            double playbackRate;
            size_t writeOffset;
        };

        template <typename DestType>
        void readAdding(ReadContext<DestType> const& ctx) {
            if (ctx.source == nullptr) {
                return;
            }

            auto const numChannels = std::min(ctx.numChannels, ctx.source->numChannels());
            auto const bufferSize = ctx.source->numSamples();
            if (numChannels == 0 || bufferSize == 0) {
                return;
            }

            // Don't process if this reader is fully faded out
            if (fade.fadedOut()) {
                return;
            }

            auto const _startOffset = ctx.startOffsetSamples.value_or(0);
            auto const _stopOffset = ctx.stopOffsetSamples.value_or(0);
            auto const startOffset = _startOffset >= 0 ?
                std::min(_startOffset, static_cast<uint64_t>(bufferSize)) : 0;
            auto const stopOffset = _stopOffset >= 0 ?
                std::min(_stopOffset, static_cast<uint64_t>(bufferSize)) : 0;
            auto const sampleLength = bufferSize - startOffset - stopOffset;

            elem::GainFade<FloatType> localFade(fade);
            elem::GainFade<FloatType> localLoopStartFade(loopStartFade);
            double pos = position;
            bool inCrossfade = inLoopCrossfade;

            // Loop range: defaults to full range if not specified
            auto const loopStart = ctx.loopRange ? ctx.loopRange->first : 0.0;
            auto const loopEnd = ctx.loopRange ? ctx.loopRange->second : 1.0;
            auto const loopLength = loopEnd - loopStart;

            // Calculate crossfade window (max 50% of loop to handle very short loops)
            auto const fadeTimeInSamples = (fadeTimeMs / 1000.0) * sampleRate;
            auto const normalizedFadeWindow = std::min(
                fadeTimeInSamples / static_cast<double>(sampleLength * loopLength),
                0.5 * loopLength
            );

            auto const loopLengthWithFade = loopEnd - loopStart - normalizedFadeWindow;
            auto const posIncrement = ctx.playbackRate / static_cast<double>(sampleLength);

            for (size_t j = 0; j < numChannels; ++j) {
                pos = position;
                localFade = fade;
                localLoopStartFade = loopStartFade;
                inCrossfade = inLoopCrossfade;

                // Here we take a subview of the buffer that ignores samples before the start offset and after the stop offset.
                // This view then gets passed into lerpRead() below. This means we can treat a pos of 0 as `startOffset` and a
                // pos of 1 as `startOffset + sampleLength`.
                auto bufferView = BufferView<float>::subview(ctx.source->getChannelData(j).data(),
                                                             startOffset, sampleLength);

                for (size_t i = 0; i < ctx.numSamples; ++i) {
                    // Check if we should enter crossfade mode
                    if (ctx.shouldLoop && !inCrossfade && pos >= (loopEnd - normalizedFadeWindow) && pos < loopEnd) {
                        inCrossfade = true;
                        localLoopStartFade.fadeIn();
                        localFade.fadeOut();
                    }

                    if (inCrossfade) {
                        // Dual-read crossfade path: derive head position from tail position
                        auto const headPos = pos - loopLengthWithFade;
                        auto const tail = localFade(lerpRead(bufferView, pos));
                        auto const head = localLoopStartFade(lerpRead(bufferView, headPos));
                        ctx.outputData[j][i + ctx.writeOffset] += static_cast<DestType>(tail + head);

                        pos += posIncrement;

                        // Exit crossfade when:
                        // 1. Tail fade completed naturally, OR
                        // 2. Tail position reached loop end before fade completed (fast playback)
                        if (localFade.fadedOut() || pos >= loopEnd) {
                            inCrossfade = false;
                            pos = pos - loopLengthWithFade;
                            localFade = localLoopStartFade;
                            // Ensure the loop fade is reset to zero before the next loop
                            localLoopStartFade.reset();
                        }
                    } else {
                        // Standard single-read path

                        if (pos >= loopEnd) {
                            if (!ctx.shouldLoop) {
                                break;
                            }
                            // Restart the loop at loop start
                            pos = loopStart + (pos - loopEnd);
                        }

                        auto const out = static_cast<DestType>(localFade(lerpRead(bufferView, pos)));
                        ctx.outputData[j][i + ctx.writeOffset] += out;

                        pos += posIncrement;
                    }
                }
            }

            // Update the fade member to have the latest state
            fade = localFade;
            loopStartFade = localLoopStartFade;
            position = pos;
            inLoopCrossfade = inCrossfade;
        }

        // Linearly interpolates between the two samples adjacent to the given position.
        // @param pos must be a normalized value between 0 and 1.
        static FloatType lerpRead(BufferView<float> const& view, double pos)
        {
            if (pos < 0.0 || pos > 1.0) {
                return FloatType(0);
            }

            auto* data = view.data();
            auto size = view.size();

            auto const realPos = pos * view.size();
            auto left = static_cast<size_t>(realPos);
            auto right = std::min(left + 1, size - 1);
            auto alpha = realPos - (double) left;

            if (left >= size)
                return FloatType(0);

            if (right >= size)
                return data[left];

            return lerp<FloatType>(static_cast<float>(alpha), data[left], data[right]);
        }

        void reset () {
            fade.reset();
            loopStartFade.reset();
            inLoopCrossfade = false;
        }

    private:
        elem::GainFade<FloatType> fade;
        double position = 0;

        // Loop crossfade state
        elem::GainFade<FloatType> loopStartFade;
        bool inLoopCrossfade = false;
        double sampleRate = 0.0;
        double fadeTimeMs = 0.0;
    };
} // namespace elem
