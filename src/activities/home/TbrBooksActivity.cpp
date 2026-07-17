#include "TbrBooksActivity.h"

#include <Arduino.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <memory>

#include "BookActions.h"
#include "CrossPointSettings.h"
#include "FileBrowserActionActivity.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "TbrBooksStore.h"
#include "activities/reader/EpubReaderActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/CompactHeader.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t MAX_LIST_TBR_BOOKS = 18;
constexpr unsigned long LONG_PRESS_MS = 1000;
}  // namespace

void TbrBooksActivity::loadTbrBooks() {
  tbrBooks.clear();
  TBR_BOOKS.pruneMissing();
  const auto& books = TBR_BOOKS.getBooks();
  tbrBooks.reserve(std::min(books.size(), MAX_LIST_TBR_BOOKS));

  for (const auto& book : books) {
    if (tbrBooks.size() >= MAX_LIST_TBR_BOOKS) {
      break;
    }
    if (!SETTINGS.showHiddenFiles && FsHelpers::containsHiddenPathSegment(book.path)) {
      continue;
    }
    tbrBooks.push_back(book);
  }
}

void TbrBooksActivity::onEnter() {
  Activity::onEnter();
  loadTbrBooks();
  selectorIndex = 0;
  requestUpdate();
}

void TbrBooksActivity::onExit() {
  Activity::onExit();
  tbrBooks.clear();
}

void TbrBooksActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = CompactHeader::contentTop(metrics);
  const int contentHeight =
      renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int pageItems = std::max(1, contentHeight / metrics.listWithSubtitleRowHeight);

  if (longPressFired) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      longPressFired = false;
    }
    return;
  }

  if (!tbrBooks.empty() && selectorIndex < tbrBooks.size() &&
      mappedInput.isPressed(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() >= LONG_PRESS_MS) {
    longPressFired = true;
    showBookActionMenu(selectorIndex, true);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!tbrBooks.empty() && selectorIndex < static_cast<int>(tbrBooks.size())) {
      LOG_DBG("TBR", "Selected TBR book: %s", tbrBooks[selectorIndex].path.c_str());
      onSelectBook(tbrBooks[selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }

  int listSize = static_cast<int>(tbrBooks.size());

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void TbrBooksActivity::reloadAfterBookAction() {
  loadTbrBooks();
  if (tbrBooks.empty()) {
    selectorIndex = 0;
  } else if (selectorIndex >= tbrBooks.size()) {
    selectorIndex = tbrBooks.size() - 1;
  }
  requestUpdate(true);
}

void TbrBooksActivity::promptDeleteBook(const RecentBook& book) {
  const std::string path = book.path;
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      return;
    }

    LOG_DBG("TBR", "Attempting to delete: %s", path.c_str());
    BookActions::clearFileMetadata(path);
    if (!Storage.remove(path.c_str())) {
      LOG_ERR("TBR", "Failed to delete file: %s", path.c_str());
      return;
    }

    TBR_BOOKS.removeByPath(path);
    RECENT_BOOKS.removeByPath(path);
    reloadAfterBookAction();
  };

  const std::string heading = tr(STR_DELETE) + std::string("? ");
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, book.title),
                         std::move(handler));
}

void TbrBooksActivity::promptRemoveFromTbr(const std::string& path, const std::string& title) {
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      return;
    }
    if (TBR_BOOKS.removeByPath(path)) {
      LOG_DBG("TBR", "Removed from TBR: %s", path.c_str());
      reloadAfterBookAction();
    }
  };

  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_TBR), title,
                                                                /*ignoreInitialConfirmRelease=*/false),
                         std::move(handler));
}

void TbrBooksActivity::showBookActionMenu(const size_t bookIndex, const bool ignoreInitialConfirmRelease) {
  if (bookIndex >= tbrBooks.size()) return;

  const RecentBook book = tbrBooks[bookIndex];
  std::vector<FileBrowserActionActivity::MenuItem> items =
      BookActions::buildBookActionItems(book.path, /*includeRemoveFromRecents=*/true);

  startActivityForResult(
      std::make_unique<FileBrowserActionActivity>(renderer, mappedInput, book.title, std::move(items),
                                                  ignoreInitialConfirmRelease),
      [this, book](const ActivityResult& result) {
        if (result.isCancelled) {
          return;
        }

        const auto* actionResult = std::get_if<FileBrowserActionResult>(&result.data);
        if (!actionResult) {
          LOG_ERR("TBR", "Book action result missing");
          return;
        }

        switch (static_cast<FileBrowserAction>(actionResult->action)) {
          case FileBrowserAction::Delete:
            promptDeleteBook(book);
            return;
          case FileBrowserAction::DeleteCache:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_CACHE), book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::clearBookCache(book.path)) {
                      LOG_ERR("TBR", "Failed to clear book cache for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_CACHE_DELETED));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::DeleteStats:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_BOOK_STATS), book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::deleteBookStats(book.path)) {
                      LOG_ERR("TBR", "Failed to delete book stats for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_STATS_DELETED));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::ResetReaderSettings:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_RESET_BOOK_READER_SETTINGS),
                    book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::resetBookReaderSettings(book.path)) {
                      LOG_ERR("TBR", "Failed to reset reader settings for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_READER_SETTINGS_RESET));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::ToggleCompleted: {
            bool completed = false;
            if (BookActions::toggleBookCompleted(book.path, book.title, completed)) {
              BookActions::drawToast(renderer, completed ? tr(STR_MARKED_FINISHED) : tr(STR_MARKED_UNFINISHED));
              delay(1000);
            }
            reloadAfterBookAction();
            return;
          }
          case FileBrowserAction::AddToTbr:
            TBR_BOOKS.addBook(book.path, book.title, book.author, book.coverBmpPath);
            BookActions::drawToast(renderer, tr(STR_ADDED_TO_TBR));
            delay(1000);
            reloadAfterBookAction();
            return;
          case FileBrowserAction::RemoveFromTbr:
            promptRemoveFromTbr(book.path, book.title);
            return;
          case FileBrowserAction::EpubRenderMode: {
            const uint8_t currentIndex =
                BookActions::epubRenderModeDisplayIndex(EpubReaderActivity::loadBookRenderMode(book.path));
            startActivityForResult(
                std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "TbrEpubRenderModeSelect",
                                                          StrId::STR_EPUB_RENDER_MODE,
                                                          BookActions::epubRenderModeOptions(), currentIndex),
                [this, book](const ActivityResult& selectionResult) {
                  if (!selectionResult.isCancelled) {
                    const auto* selection = std::get_if<OptionSelectionResult>(&selectionResult.data);
                    if (selection != nullptr &&
                        !EpubReaderActivity::saveBookRenderMode(
                            book.path, BookActions::epubRenderModeForDisplayIndex(selection->index))) {
                      LOG_ERR("TBR", "Failed to save render mode for: %s", book.path.c_str());
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          }
          case FileBrowserAction::RemoveFromRecents:
            RECENT_BOOKS.removeByPath(book.path);
            BookActions::drawToast(renderer, tr(STR_REMOVED_FROM_RECENTS));
            delay(1000);
            reloadAfterBookAction();
            return;
          case FileBrowserAction::PinFavorite:
          case FileBrowserAction::UnpinFavorite:
          case FileBrowserAction::SetSleepFolder:
          case FileBrowserAction::ClearSleepFolder:
          case FileBrowserAction::ViewBookmarks:
          case FileBrowserAction::ViewClippings:
          case FileBrowserAction::DeleteBookmarks:
          case FileBrowserAction::DeleteClippings:
            return;
        }
      });
}

void TbrBooksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  CompactHeader::drawTitle(renderer, tr(STR_MENU_TBR));

  const int contentTop = CompactHeader::contentTop(metrics);
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (tbrBooks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_TBR_BOOKS));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, tbrBooks.size(), selectorIndex,
        [this](int index) { return tbrBooks[index].title; }, [this](int index) { return tbrBooks[index].author; },
        [this](int index) { return UITheme::getFileIcon(tbrBooks[index].path); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
