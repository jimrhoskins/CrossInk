#pragma once

#include "components/themes/lyra/LyraTheme.h"

class GfxRenderer;

namespace LyraMinimalMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = LyraMetrics::values;
  v.homeRecentBooksCount = 0;
  v.homeContinueReadingInMenu = true;
  v.homeCoverHeight = 0;
  v.homeCoverTileHeight = 0;
  return v;
}();
}  // namespace LyraMinimalMetrics

class LyraMinimalTheme : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats = nullptr,
                           float progressPercent = -1.0f, const GlobalReadingStats* globalStats = nullptr,
                           const char* currentChapterTitle = nullptr) const override;
  const char* homeHeaderTitle(const std::vector<RecentBook>& recentBooks, bool continueReadingInMenu) const override;
};
