#include "db_handler.h"
#include "sys_tools.h"

#include <sqlite3.h>

#include <iostream>
#include <filesystem>

// Function to execute an SQL command without fetching results
void executeSQL(sqlite3* db, const std::string& sql) {
    char* errorMessage = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errorMessage);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL Error: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to execute SQL statement");
    }
}

// Function to create the database schema if it does not exist
void createHistoryDatabase(sqlite3* db) {
    const std::string createTablesSQL =
        "CREATE TABLE IF NOT EXISTS command ("
        "  id INTEGER PRIMARY KEY,"
        "  command VARCHAR(255)"
        ");"
        "CREATE TABLE IF NOT EXISTS location ("
        "  id INTEGER PRIMARY KEY,"
        "  location VARCHAR(255)"
        ");"
        "CREATE TABLE IF NOT EXISTS command_location ("
        "  created_at DATETIME,"
        "  command_id INTEGER,"
        "  location_id INTEGER,"
        "  PRIMARY KEY (created_at),"
        "  FOREIGN KEY (command_id) REFERENCES command (id),"
        "  FOREIGN KEY (location_id) REFERENCES location (id)"
        ");";

    executeSQL(db, createTablesSQL); // Create tables if not existing
}


// Function to execute an SQL query and return results with a callback
int executeQuery(sqlite3* db, const std::string& sql, int (*callback)(void*, int, char**, char**), void* data) {
    char* errorMessage = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), callback, data, &errorMessage);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL Error: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to execute SQL query");
    }

    return rc;
}

// Callback function to extract command and location from the query result
int commandLocationCallback(void* resultTuple, int argc, char** argv, char** colNames) {
    if (argc != 2) {
        throw std::runtime_error("Unexpected number of columns");
    }

    std::tuple<std::string, std::string>* result = static_cast<std::tuple<std::string, std::string>*>(resultTuple);

    // Store the command and location into the tuple
    std::get<0>(*result) = argv[0] ? argv[0] : ""; // command
    std::get<1>(*result) = argv[1] ? argv[1] : ""; // location

    return 0; // Success
}
// Function to retrieve a command-location pair from command_location table based on a given counter
std::tuple<std::string, std::string> getCommandLocation(sqlite3* db, int counter)
{
    std::tuple<std::string, std::string> result;

    // SQL query to get the desired row from command_location and join with command and location
    std::string query =
        "SELECT command.command, location.location "
        "FROM command_location "
        "JOIN command ON command_location.command_id = command.id "
        "JOIN location ON command_location.location_id = location.id "
        "ORDER BY command_location.created_at DESC "
        "LIMIT 1 OFFSET " + std::to_string(counter) + ";"; // Get the row by counter as offset

    executeQuery(db, query, commandLocationCallback, &result);

    return result;
}


// Function to retrieve a command-location pair from command_location table based on a given counter and a specific location
std::tuple<std::string, std::string> getCommandLocation(sqlite3* db, int counter, const std::string& locationName)
{
    std::tuple<std::string, std::string> result;

    // First, find the location ID that matches the given location name
    std::string findLocationIdQuery = "SELECT id FROM location WHERE location = '" + locationName + "';";
    int locationId = -1;
    auto locationIdCallback = [](void* idPtr, int argc, char** argv, char** azColName) -> int {
        if (argc == 1) {
            *static_cast<int*>(idPtr) = std::stoi(argv[0]);
        }
        return 0;
    };

    executeQuery(db, findLocationIdQuery, locationIdCallback, &locationId);

    if (locationId == -1) {
        std::cerr << "No location found with the name: " << locationName << std::endl;
        throw std::runtime_error("Location not found");
    }

    // SQL query to get the desired row from command_location and join with command and location, filtering by the location ID
    std::string query =
        "SELECT command.command, location.location "
        "FROM command_location "
        "JOIN command ON command_location.command_id = command.id "
        "JOIN location ON command_location.location_id = location.id "
        "WHERE location.id = " + std::to_string(locationId) + " "
                                       "ORDER BY command_location.created_at DESC "
                                       "LIMIT 1 OFFSET " + std::to_string(counter) + ";"; // Get the row by counter as offset

    executeQuery(db, query, commandLocationCallback, &result);

    return result;
}

// Helper function to get the ID from a table if a record exists, or return -1 if it doesn't
int getID(sqlite3* db, const std::string& table, const std::string& column, const std::string& value) {
    sqlite3_stmt* stmt = nullptr;
    std::string query = "SELECT id FROM " + table + " WHERE " + column + " = ?;";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) { // If there's a result, fetch the ID
        id = sqlite3_column_int(stmt, 0); // Get the first column (ID)
    }
    sqlite3_finalize(stmt);
    return id;
}

// Function to insert or fetch the command and location, then link them in command_location
void insertOrFetchCommandLocation(sqlite3* db, const std::string& command, const std::string& location) {
    sqlite3_stmt* stmt = nullptr;

    try {
        // Start a transaction
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        // Check if the command exists, otherwise insert and fetch ID
        int command_id = getID(db, "command", "command", command);
        if (command_id == -1) { // Command does not exist, insert it
            const char* insertCommandSQL = "INSERT INTO command (command) VALUES (?);";
            sqlite3_prepare_v2(db, insertCommandSQL, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, command.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            command_id = sqlite3_last_insert_rowid(db); // Get the new ID
        }

        // Check if the location exists, otherwise insert and fetch ID
        int location_id = getID(db, "location", "location", location);
        if (location_id == -1) { // Location does not exist, insert it
            const char* insertLocationSQL = "INSERT INTO location (location) VALUES (?);";
            sqlite3_prepare_v2(db, insertLocationSQL, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, location.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            location_id = sqlite3_last_insert_rowid(db); // Get the new ID
        }

        // Insert into command_location with the corresponding command_id and location_id
        const char* insertCommandLocationSQL =
            "INSERT INTO command_location (created_at, command_id, location_id) "
            "VALUES (datetime('now'), ?, ?);";
        sqlite3_prepare_v2(db, insertCommandLocationSQL, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, command_id); // command_id
        sqlite3_bind_int(stmt, 2, location_id); // location_id
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Commit the transaction
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    } catch (const std::exception& ex) {
        // If there is an error, rollback the transaction
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        std::cerr << "Transaction failed: " << ex.what() << std::endl;
        throw;
    }
}

/**
 * @brief storeCommandHistory Will store the current location and the given command in the history database
 * @param command the command to store
 * @return
 */
bool storeCommandHistory(const std::string& command)
{
    namespace fs = std::filesystem;

    sqlite3* db = nullptr;
    int rc = sqlite3_open(getHistoryDatabase().c_str(), &db); // Open or create the database

    if (rc)
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    try
    {
        // Insert a command and a location, or fetch existing IDs, then link them in command_location
        insertOrFetchCommandLocation(db, command, fs::current_path());
    }
    catch (const std::exception& ex)
    {
        std::cerr << "An error occurred: " << ex.what() << std::endl;
        return false;
    }

    sqlite3_close(db); // Close the database
    return true;
}

std::string getHistoryDatabase()
{
    return getExecutablePath() + "/lxf-shell-history.db";
}
