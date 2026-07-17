#pragma once
#include <PersistableStore.h>

#include <string>
#include <vector>

#include "RecentBooksStore.h"

class TbrBooksStore : public PersistableStore<TbrBooksStore> {
 private:
  std::vector<RecentBook> books;

  static constexpr int MAX_TBR_BOOKS = 18;

  TbrBooksStore() = default;
  ~TbrBooksStore() = default;

  friend class PersistableStore<TbrBooksStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/tbr.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Add a book to the end of the TBR list. No-op if already present.
  void addBook(const std::string& path, const std::string& title, const std::string& author,
               const std::string& coverBmpPath);

  // Remove the entry whose path matches. Returns true if removed.
  bool removeByPath(const std::string& path);

  // True if the given path is in the TBR list.
  bool isInTbr(const std::string& path) const;

  // True if the book's backing file is no longer present on the SD card.
  static bool isMissing(const RecentBook& book);

  // Remove entries whose backing file is no longer on the SD card.
  // Returns true if any entry was removed. Does not persist — caller decides.
  bool pruneMissing();

  const std::vector<RecentBook>& getBooks() const { return books; }
  int getCount() const { return static_cast<int>(books.size()); }
  bool isEmpty() const { return books.empty(); }
};

#define TBR_BOOKS TbrBooksStore::getInstance()
