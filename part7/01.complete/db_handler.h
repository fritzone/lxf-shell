#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include <tuple>
#include <string>

struct sqlite3;


/**
 * @brief getHistoryDatabase() returns the path of the history database file
 */
std::string getHistoryDatabase();

void insertOrFetchCommandLocation(sqlite3* db, const std::string& command, const std::string& location);

int getID(sqlite3* db, const std::string& table, const std::string& column, const std::string& value);

bool storeCommandHistory(const std::string& command);

std::tuple<std::string, std::string> getCommandLocation(sqlite3* db, int counter);

std::tuple<std::string, std::string> getCommandLocation(sqlite3* db, int counter, const std::string& locationName);

void createHistoryDatabase(sqlite3* db);

#endif // DB_HANDLER_H
