#include "TbrBooksStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

void TbrBooksStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& book : books) {
    JsonObject obj = arr.add<JsonObject>();
    obj["path"] = book.path;
    obj["title"] = book.title;
    obj["author"] = book.author;
    obj["coverBmpPath"] = book.coverBmpPath;
  }
}

bool TbrBooksStore::fromJson(JsonVariantConst doc) {
  books.clear();
  JsonArrayConst arr = doc["books"].as<JsonArrayConst>();
  books.reserve(std::min(arr.size(), static_cast<size_t>(MAX_TBR_BOOKS)));
  for (JsonObjectConst obj : arr) {
    if (getCount() >= MAX_TBR_BOOKS) break;
    RecentBook book;
    book.path = obj["path"] | "";
    book.title = obj["title"] | "";
    book.author = obj["author"] | "";
    book.coverBmpPath = obj["coverBmpPath"] | "";
    books.push_back(book);
  }

  LOG_DBG("TBR", "TBR books loaded from file (%d entries)", getCount());
  return true;
}

void TbrBooksStore::addBook(const std::string& path, const std::string& title, const std::string& author,
                            const std::string& coverBmpPath) {
  if (isInTbr(path)) {
    return;
  }
  books.push_back({path, title, author, coverBmpPath});
  if (books.size() > MAX_TBR_BOOKS) {
    books.resize(MAX_TBR_BOOKS);
  }
  saveToFile();
}

bool TbrBooksStore::removeByPath(const std::string& path) {
  auto it = std::find_if(books.begin(), books.end(), [&](const RecentBook& book) { return book.path == path; });
  if (it == books.end()) {
    return false;
  }
  books.erase(it);
  if (!saveToFile()) {
    LOG_ERR("TBR", "Failed to persist removal of TBR book: %s", path.c_str());
  }
  return true;
}

bool TbrBooksStore::isInTbr(const std::string& path) const {
  return std::any_of(books.begin(), books.end(), [&](const RecentBook& book) { return book.path == path; });
}

bool TbrBooksStore::isMissing(const RecentBook& book) { return !Storage.exists(book.path.c_str()); }

bool TbrBooksStore::pruneMissing() {
  const size_t before = books.size();
  books.erase(std::remove_if(books.begin(), books.end(), &isMissing), books.end());
  return books.size() != before;
}
