#pragma once

#include <string>

#include <elem/Runtime.h>

/*
 * Your main can call this function to render an Elementary graph to disk for
 * the given duration.
 */
template <typename FloatType>
bool runOffline(std::string const& inputFileName,
                std::string const& outputFileName,
                double durationSeconds);
