#pragma once
#include <I18n.h>

#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class TbrBooksActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;

  bool longPressFired = false;

  std::vector<RecentBook> tbrBooks;

  void loadTbrBooks();
  void reloadAfterBookAction();

  void promptDeleteBook(const RecentBook& book);
  void promptRemoveFromTbr(const std::string& path, const std::string& title);
  void showBookActionMenu(size_t bookIndex, bool ignoreInitialConfirmRelease = false);

 public:
  explicit TbrBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TbrBooks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
