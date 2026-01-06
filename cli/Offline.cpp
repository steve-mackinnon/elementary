#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

#include <choc_Files.h>
#include <choc_javascript.h>
#include <choc_javascript_QuickJS.h>

#include "Offline.h"

#include "choc/audio/choc_AudioFileFormat_WAV.h"
#include "choc/audio/choc_SampleBuffers.h"

#include <elem/AudioBufferResource.h>


namespace {

constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr size_t kNumOutputChannels = 2;
constexpr const char* kTestToneName = "testtone";

const auto* kConsoleShimScript = R"script(
(function() {
  if (typeof globalThis.console === 'undefined') {
    globalThis.console = {
      log(...args) {
        return __log__('[log]', ...args);
      },
      warn(...args) {
        return __log__('[warn]', ...args);
      },
      error(...args) {
        return __log__('[error]', ...args);
      },
    };
  }
})();
)script";

std::filesystem::path getTestTonePath()
{
    auto sourceDir = std::filesystem::path(__FILE__).parent_path();
    return sourceDir / "test_assets" / "test.wav";
}

template <typename FloatType>
bool addTestTone(elem::Runtime<FloatType>& runtime)
{
    choc::audio::WAVAudioFileFormat<false> wavFormat;
    auto testTonePath = getTestTonePath().string();
    auto reader = wavFormat.createReader(testTonePath);

    if (!reader) {
        std::cout << "Failed to load test tone: " << testTonePath << std::endl;
        return false;
    }

    auto buffer = reader->readEntireStream<float>();
    auto view = buffer.getView();

    if (view.getNumChannels() == 0 || view.getNumFrames() == 0) {
        std::cout << "Test tone is empty: " << testTonePath << std::endl;
        return false;
    }

    std::vector<float*> channelPointers;
    channelPointers.reserve(view.getNumChannels());

    for (choc::buffer::ChannelCount i = 0; i < view.getNumChannels(); ++i) {
        channelPointers.push_back(view.data.channels[i]);
    }

    auto resource = std::make_unique<elem::AudioBufferResource>(
        channelPointers.data(),
        channelPointers.size(),
        view.getNumFrames());

    if (!runtime.addSharedResource(kTestToneName, std::move(resource))) {
        std::cout << "Failed to add shared resource: " << kTestToneName << std::endl;
        return false;
    }

    return true;
}

} // namespace

template <typename FloatType>
bool runOffline(std::string const& inputFileName,
                std::string const& outputFileName,
                double durationSeconds)
{
    if (durationSeconds <= 0.0) {
        std::cout << "Duration must be greater than zero." << std::endl;
        return false;
    }

    elem::Runtime<FloatType> runtime(kSampleRate, kBlockSize);

    if (!addTestTone(runtime))
        return false;

    auto ctx = choc::javascript::createQuickJSContext();

    ctx.registerFunction("__postNativeMessage__", [&](choc::javascript::ArgumentList args) {
        runtime.applyInstructions(elem::js::parseJSON(args[0]->toString()));
        return choc::value::Value();
    });

    ctx.registerFunction("__log__", [](choc::javascript::ArgumentList args) {
        for (size_t i = 0; i < args.numArgs; ++i) {
            std::cout << choc::json::toString(*args[i], true) << std::endl;
        }

        return choc::value::Value();
    });

    // Shim the js environment for console logging
    (void) ctx.evaluate(kConsoleShimScript);

    // Execute the input file
    auto inputFile = choc::file::loadFileAsString(inputFileName);
    (void) ctx.evaluate(inputFile);

    auto const totalFrames = static_cast<uint64_t>(durationSeconds * kSampleRate);
    if (totalFrames == 0) {
        std::cout << "Duration is too small to render any audio." << std::endl;
        return false;
    }

    choc::audio::AudioFileProperties props;
    props.formatName = "WAV";
    props.sampleRate = kSampleRate;
    props.numFrames = totalFrames;
    props.numChannels = static_cast<uint32_t>(kNumOutputChannels);
    props.bitDepth = choc::audio::BitDepth::float32;

    choc::audio::WAVAudioFileFormat<true> outputFormat;
    auto writer = outputFormat.createWriter(outputFileName, props);

    if (!writer) {
        std::cout << "Failed to open output file: " << outputFileName << std::endl;
        return false;
    }

    std::vector<std::vector<FloatType>> scratchBuffers;
    std::vector<FloatType*> scratchPointers;

    for (size_t i = 0; i < kNumOutputChannels; ++i) {
        scratchBuffers.push_back(std::vector<FloatType>(kBlockSize));
        scratchPointers.push_back(scratchBuffers[i].data());
    }

    uint64_t framesRendered = 0;

    while (framesRendered < totalFrames) {
        auto const framesThisBlock = static_cast<size_t>(
            std::min<uint64_t>(kBlockSize, totalFrames - framesRendered));

        runtime.process(
            nullptr,
            0,
            scratchPointers.data(),
            kNumOutputChannels,
            framesThisBlock,
            0
        );

        auto view = choc::buffer::createChannelArrayView(
            scratchPointers.data(),
            static_cast<choc::buffer::ChannelCount>(kNumOutputChannels),
            static_cast<choc::buffer::FrameCount>(framesThisBlock));

        if (!writer->appendFrames(view)) {
            std::cout << "Failed while writing output audio." << std::endl;
            return false;
        }

        framesRendered += framesThisBlock;
    }

    writer->flush();
    std::cout << "Wrote " << outputFileName << " (" << durationSeconds << "s)" << std::endl;
    return true;
}

template bool runOffline<float>(std::string const& inputFileName,
                                std::string const& outputFileName,
                                double durationSeconds);
template bool runOffline<double>(std::string const& inputFileName,
                                 std::string const& outputFileName,
                                 double durationSeconds);
