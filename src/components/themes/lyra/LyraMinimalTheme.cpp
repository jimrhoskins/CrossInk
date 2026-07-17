#include "LyraMinimalTheme.h"

#include <GfxRenderer.h>

#include <cstdint>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "fontIds.h"

void LyraMinimalTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect,
                                           const std::vector<RecentBook>& /*recentBooks*/, int /*selectorIndex*/,
                                           bool& /*coverRendered*/, bool& /*coverBufferStored*/,
                                           bool& /*bufferRestored*/, const std::function<bool()>& /*storeCoverBuffer*/,
                                           const BookReadingStats* /*stats*/, float /*progressPercent*/,
                                           const GlobalReadingStats* /*globalStats*/,
                                           const char* /*currentChapterTitle*/) const {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, Color::White);
}

const char* LyraMinimalTheme::homeHeaderTitle(const std::vector<RecentBook>& /*recentBooks*/,
                                              bool /*continueReadingInMenu*/) const {
  return nullptr;
}
