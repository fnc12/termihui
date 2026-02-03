#pragma once

#include <termihui/text_style.h>
#include <pangomm.h>
#include <string>
#include <vector>

/// Convert Color to Pango color string (e.g. "#FF0000")
std::string colorToPango(const Color& color);

/// Convert StyledSegment to Pango markup
std::string segmentToPangoMarkup(const StyledSegment& segment);

/// Convert vector of StyledSegments to Pango markup
std::string segmentsToPangoMarkup(const std::vector<StyledSegment>& segments);

/// Get plain text from segments
std::string segmentsToPlainText(const std::vector<StyledSegment>& segments);
