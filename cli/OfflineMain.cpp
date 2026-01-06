#include <iostream>
#include <string>

#include "Offline.h"

int main(int argc, char **argv)
{
    auto printUsage = [](const char* exe) {
        std::cout << "Usage: " << exe << " <script.js> <output.wav> [durationSeconds]" << std::endl;
        std::cout << "Renders an Elementary graph to a WAV file." << std::endl;
    };

    if (argc >= 2) {
        auto arg = std::string(argv[1]);
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (argc < 3) {
        std::cout << "Missing arguments." << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    auto inputFileName = std::string(argv[1]);
    auto outputFileName = std::string(argv[2]);
    double durationSeconds = 5.0;

    if (argc >= 4) {
        try {
            durationSeconds = std::stod(argv[3]);
        } catch (const std::exception&) {
            std::cout << "Invalid duration: " << argv[3] << std::endl;
            return 1;
        }
    }

    if (durationSeconds <= 0.0) {
        std::cout << "Duration must be greater than zero." << std::endl;
        return 1;
    }

    auto ok = runOffline<float>(
        inputFileName,
        outputFileName,
        durationSeconds);

    return ok ? 0 : 1;
}
