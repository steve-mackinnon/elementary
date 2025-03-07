#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"
#include "../Types.h"

#include "helpers/RefCountedPool.h"
#include "../third-party/signalsmith-stretch/signalsmith-stretch.h"

#include <map>
#include <iostream>


namespace elem
{

    namespace detail
    {
        template <typename FloatType>
        FloatType lerp (FloatType alpha, FloatType x, FloatType y) {
            return x + alpha * (y - x);
        }

        template <typename FloatType>
        FloatType fpEqual (FloatType x, FloatType y) {
            return std::abs(x - y) <= FloatType(1e-6);
        }

        template <typename FloatType>
        struct GainFade {
            GainFade() = default;

            void setTargetGain (FloatType g) {
                targetGain = g;

                if (targetGain < currentGain) {
                    step = FloatType(-1) * std::abs(step);
                } else {
                    step = std::abs(step);
                }
            }

            FloatType operator() (FloatType x) {
                if (currentGain == targetGain)
                    return (currentGain * x);

                auto y = x * currentGain;
                currentGain = std::clamp(currentGain + step, FloatType(0), FloatType(1));

                return y;
            }

            bool on() {
                return fpEqual(targetGain, FloatType(1));
            }

            bool silent() {
                return fpEqual(targetGain, FloatType(0)) && fpEqual(currentGain, FloatType(0));
            }

            void reset() {
                currentGain = FloatType(0);
                targetGain = FloatType(0);
            }

            FloatType currentGain = 0;
            FloatType targetGain = 0;
            FloatType step = 0.02; // TODO
        };

        template <typename FloatType>
        struct BufferReader {
            BufferReader() = default;

            void engage (double start, double currentTime, FloatType* _buffer, size_t _size) {
                startTime = start;
                buffer = _buffer;
                bufferSize = _size;
                fade.setTargetGain(FloatType(1));

                position = static_cast<size_t>(((currentTime - startTime) / sampleDuration * (double) (bufferSize - 1u))) + startOffset;
                position = std::clamp<size_t>(position, startOffset, bufferSize - stopOffset);
            }

            void disengage() {
                fade.setTargetGain(FloatType(0));
            }

            // Does the incoming time match what this reader is expecting?
            //
            // If we're not engaged, we don't have any expectations so we just say sure.
            // If we are engaged, we try to map the incoming time onto a position in the
            // buffer and see if that's far off from where we currently are.
            bool isAlignedWithTime(double t) {
                if (!fade.on())
                    return true;

                size_t newPos = static_cast<size_t>(((t - startTime) / sampleDuration) * (double) (bufferSize - 1u));
                int delta = static_cast<int>(position) - static_cast<int>(newPos);
                bool aligned = std::abs(delta) < 16;

                return aligned;
            }

            template <typename DestType>
            void readAdding(DestType* outputData, size_t numSamples) {
                for (size_t i = 0; (i < numSamples) && (position < bufferSize - stopOffset); ++i) {
                    if (position < startOffset) {
                        ++position;
                        continue;
                    }
                    outputData[i] += static_cast<DestType>(fade(buffer[position++]));
                }
            }

            FloatType stepStopTime() {
                lastTimeStep += dt;
                return lastTimeStep;
            }

            void reset (double sampleDur) {
                fade.reset();

                sampleDuration = sampleDur;
                startTime = 0.0;
                dt = 0.0;
            }

            void setOffsets(size_t start, size_t stop) {
                startOffset = start;
                stopOffset = stop;
            }

            GainFade<FloatType> fade;
            FloatType* buffer = nullptr;
            size_t bufferSize = 0;
            size_t position = 0;

            double sampleDuration = 0;
            double startTime = 0;
            double lastTimeStep = 0;
            double dt = 0;
            size_t startOffset = 0;
            size_t stopOffset = 0;
        };
    }

    template <typename FloatType, bool WithStretch = false>
    struct SampleSeqNode : public GraphNode<FloatType> {
        SampleSeqNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            if constexpr (WithStretch) {
                stretch.presetDefault(1, sr);
                scratchBuffer.resize(blockSize * 4);
            }
        }


        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap& resources) override
        {
            if constexpr (WithStretch) {
                if (key == "shift") {
                    if (!val.isNumber())
                        return ReturnCode::InvalidPropertyType();

                    auto shift = (js::Number) val;
                    stretch.setTransposeSemitones(shift);
                }

                if (key == "stretch") {
                    if (!val.isNumber())
                        return ReturnCode::InvalidPropertyType();

                    auto _stretchFactor = (js::Number) val;

                    if (_stretchFactor < 0.25 || _stretchFactor > 4.0)
                        return ReturnCode::InvalidPropertyValue();

                    stretchFactor.store(_stretchFactor);
                }
            }

            if (key == "duration") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto dur = (js::Number) val;

                if (dur <= 0.0)
                    return ReturnCode::InvalidPropertyValue();

                sampleDuration.store(dur);
            }

            if (key == "path") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();

                auto data = seqPool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence
                data->clear();

                // We expect from the JavaScript side an array of event objects, where each
                // event includes a value to take and a time at which to take that value
                for (size_t i = 0; i < seq.size(); ++i) {
                    auto& event = seq[i].getObject();

                    FloatType value = static_cast<FloatType>((js::Number) event.at("value"));
                    double time = static_cast<double>((js::Number) event.at("time"));

                    data->insert({ time, value });
                }

                seqQueue.push(std::move(data));
            }

            if (key == "startOffset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                if (vi < 0)
                    return ReturnCode::InvalidPropertyValue();

                startOffset.store(static_cast<size_t>(vi));
            }

            if (key == "stopOffset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                if (vi < 0)
                    return ReturnCode::InvalidPropertyValue();

                stopOffset.store(static_cast<size_t>(vi));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void updateEventBoundaries(double t) {
            nextEvent = activeSeq->upper_bound(t);

            // The next event is the first one in the sequence
            if (nextEvent == activeSeq->begin()) {
                prevEvent = activeSeq->end();

                // Here we know that nothing should be playing, so, easy:
                readers[0].disengage();
                readers[1].disengage();
            } else {
                prevEvent = std::prev(nextEvent);

                // And here we decide based on the previous event
                readers[activeReader].disengage();
                activeReader = (activeReader + 1) & (readers.size() - 1);

                // Here a value of 1.0 is considered an onset, and anything else
                // considered an offset.
                if (detail::fpEqual(prevEvent->second, FloatType(1.0))) {
                    auto const bufferView = activeBuffer->getChannelData(0);
                    readers[activeReader].engage(prevEvent->first, t, const_cast<float*>(bufferView.data()), bufferView.size());
                }
            }
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData[0];
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // Load sample duration
            auto const sampleDur = sampleDuration.load();

            if (sampleDur != rtSampleDuration) {
                readers[0].reset(sampleDur);
                readers[1].reset(sampleDur);
                rtSampleDuration = sampleDur;
            }

            auto const startOffset_ = startOffset.load();
            auto const stopOffset_ = stopOffset.load();
            if (startOffset_ != rtStartOffset || stopOffset_ != rtStopOffset) {
                readers[0].setOffsets(startOffset, stopOffset);
                readers[1].setOffsets(startOffset, stopOffset);
                rtStartOffset = startOffset_;
                rtStopOffset = stopOffset_;
            }

            // Pull newest buffer from queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);

                readers[0].reset(sampleDur);
                readers[1].reset(sampleDur);
            }

            // Pull newest seq from queue
            if (seqQueue.size() > 0) {
                while (seqQueue.size() > 0) {
                    seqQueue.pop(activeSeq);
                }

                // New sequence means we'll have to find our new event boundaries given
                // the current input time
                prevEvent = activeSeq->end();
                nextEvent = activeSeq->end();
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSeq == nullptr || activeSeq->size() == 0 || activeBuffer == nullptr || sampleDur <= 0.0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We reference this a lot
            auto const seqEnd = activeSeq->end();
            auto* scratchData = scratchBuffer.data();

            // Helpers to add some tolerance to the time checks
            auto const before = [](double t1, double t2) { return t1 <= (t2 + 1e-6); };
            auto const after = [](double t1, double t2) { return t1 >= (t2 - 1e-6); };

            // Downsampling from a-rate to k-rate
            auto const t = static_cast<double>(inputData[0][0]);

            // We update our event boundaries if we just took a new sequence, if we've stepped
            // forwards or backwards over the next event time, or if the incoming time step differs
            // excessively from what we expected
            auto const shouldUpdateBounds = (prevEvent == seqEnd && nextEvent == seqEnd)
                || (prevEvent != seqEnd && before(t, prevEvent->first))
                || (nextEvent != seqEnd && after(t, nextEvent->first));

            // TODO: if the input time has changed significantly, need to address the input latency of
            // the phase vocoder by resetting it and then pushing stretch.inputLatency * stretchFactor
            // samples ahead of `timeInSamples(t)`
            if (shouldUpdateBounds || !readers[activeReader].isAlignedWithTime(t)) {
                updateEventBoundaries(t);
            }

            if constexpr (WithStretch) {
                // Some fractional sample counting here. Every time we calculate the number of
                // source samples, we inevitably leave a little rounding error. To ensure we
                // average out correctly over time, we accumulate that rounding error and nudge
                // our numSourceSamples once the accumulated error exceeds a full sample.
                double const trueSourceSamples = (double) numSamples / stretchFactor.load();
                size_t numSourceSamples = static_cast<size_t>(trueSourceSamples);

                accFracSamples += (trueSourceSamples - (double) numSourceSamples);

                if (accFracSamples >= 1.0) {
                    accFracSamples -= 1.0;
                    numSourceSamples++;
                }

                numSourceSamples = std::clamp(numSourceSamples, static_cast<size_t>(0), scratchBuffer.size());

                // Clear and read
                std::fill_n(scratchData, numSourceSamples, FloatType(0));

                readers[0].readAdding(scratchData, numSourceSamples);
                readers[1].readAdding(scratchData, numSourceSamples);

                stretch.process(&scratchData, numSourceSamples, &outputData, numSamples);
            } else {
                // Clear and read
                std::fill_n(outputData, numSamples, FloatType(0));

                readers[0].readAdding(outputData, numSamples);
                readers[1].readAdding(outputData, numSamples);
            }
        }

        using Sequence = std::map<double, FloatType, std::less<double>>;

        RefCountedPool<Sequence> seqPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<Sequence>> seqQueue;
        std::shared_ptr<Sequence> activeSeq;

        typename Sequence::iterator prevEvent;
        typename Sequence::iterator nextEvent;

        SingleWriterSingleReaderQueue<SharedResourcePtr> bufferQueue;
        SharedResourcePtr activeBuffer;

        std::array<detail::BufferReader<float>, 2> readers;
        size_t activeReader = 0;
        int64_t nextExpectedBlockStart = 0;

        std::atomic<double> sampleDuration = 0;
        double rtSampleDuration = 0;

        signalsmith::stretch::SignalsmithStretch<FloatType> stretch;
        double accFracSamples = 0;
        std::atomic<double> stretchFactor = 1.0;
        std::vector<FloatType> scratchBuffer;

        std::atomic<size_t> startOffset = 0;
        size_t rtStartOffset = 0;
        std::atomic<size_t> stopOffset = 0;
        size_t rtStopOffset = 0;
    };

    template <typename FloatType>
    using SampleSeqWithStretchNode = SampleSeqNode<FloatType, true>;

} // namespace elem
