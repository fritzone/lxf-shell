#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <utils.h>

TEST_CASE("Empty string results in an empty vector", "[splitStringByWhitespace]")
{
    std::string input = "";
    auto result = splitStringByWhitespace(input);
    REQUIRE(result.empty());
}

TEST_CASE("String with one word results in a vector with one element", "[splitStringByWhitespace]")
{
    std::string input = "Hello";
    auto result = splitStringByWhitespace(input);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == "Hello");
}

TEST_CASE("String with multiple words separated by whitespace", "[splitStringByWhitespace]")
{
    std::string input = "This is a test";
    auto result = splitStringByWhitespace(input);
    REQUIRE(result.size() == 4);
    REQUIRE(result[0] == "This");
    REQUIRE(result[1] == "is");
    REQUIRE(result[2] == "a");
    REQUIRE(result[3] == "test");
}

TEST_CASE("String with leading and trailing whitespace", "[splitStringByWhitespace]")
{
    std::string input = "   leading trailing   ";
    auto result = splitStringByWhitespace(input);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == "leading");
    REQUIRE(result[1] == "trailing");
}

TEST_CASE("String with consecutive whitespace characters", "[splitStringByWhitespace]")
{
    std::string input = "a  b   c";
    auto result = splitStringByWhitespace(input);
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "a");
    REQUIRE(result[1] == "b");
    REQUIRE(result[2] == "c");
}

TEST_CASE("Empty string with any delimiter results in an empty vector", "[splitStringWithDelimiter]")
{
    std::string input = "";
    std::string delimiter = ",";
    auto result = splitStringWithDelimiter(input, delimiter);
    REQUIRE(result.empty());
}

TEST_CASE("String with no delimiter results in a vector with one element", "[splitStringWithDelimiter]")
{
    std::string input = "Hello";
    std::string delimiter = ",";
    auto result = splitStringWithDelimiter(input, delimiter);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == "Hello");
}

TEST_CASE("String with a single character delimiter", "[splitStringWithDelimiter]")
{
    std::string input = "apple,orange,banana";
    std::string delimiter = ",";
    auto result = splitStringWithDelimiter(input, delimiter);
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "apple");
    REQUIRE(result[1] == "orange");
    REQUIRE(result[2] == "banana");
}

TEST_CASE("String with a multi-character delimiter", "[splitStringWithDelimiter]")
{
    std::string input = "one-two-three";
    std::string delimiter = "-";
    auto result = splitStringWithDelimiter(input, delimiter);
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "one");
    REQUIRE(result[1] == "two");
    REQUIRE(result[2] == "three");
}

TEST_CASE("String with consecutive delimiters", "[splitStringWithDelimiter]")
{
    std::string input = "apple,,orange,,banana";
    std::string delimiter = ",,";
    auto result = splitStringWithDelimiter(input, delimiter);
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "apple");
    REQUIRE(result[1] == "orange");
    REQUIRE(result[2] == "banana");
}

TEST_CASE("String with leading and trailing delimiters", "[splitStringWithDelimiter]")
{
    std::string input = ",,apple,orange,banana,,";
    std::string delimiter = ",";
    auto result = splitStringWithDelimiter(input, delimiter);
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "apple");
    REQUIRE(result[1] == "orange");
    REQUIRE(result[2] == "banana");
}


TEST_CASE("Extract words after sequence with occurrences", "[extractWordsAfterSequence]")
{
    std::string input = "This is a sequence: word1 :word2: word3";
    std::string sequence = ":";
    auto result = extractWordsAfterSequence(input, sequence);
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "word1");
    REQUIRE(result[1] == "word2");
    REQUIRE(result[2] == "word3");
    REQUIRE(input == "This is a sequence ");
}

TEST_CASE("Extract words after sequence with multiple occurrences", "[extractWordsAfterSequence]")
{
    std::string input = "sequence: word1 word2. Another sequence: word3 word4.";
    std::string sequence = "sequence:";
    auto result = extractWordsAfterSequence(input, sequence);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == "word1");
    REQUIRE(result[1] == "word3");
    REQUIRE(input == " word2. Another  word4.");
}

TEST_CASE("Extract words after non-existent sequence", "[extractWordsAfterSequence]")
{
    std::string input = "No sequence here. Just words.";
    std::string sequence = "nonexistent";
    auto result = extractWordsAfterSequence(input, sequence);
    REQUIRE(result.empty());
    REQUIRE(input == "No sequence here. Just words.");
}

TEST_CASE("Extract words after sequence with spaces around", "[extractWordsAfterSequence]")
{
    std::string input = "Sequence :  word1   word2   word3.";
    std::string sequence = ":";
    auto result = extractWordsAfterSequence(input, sequence);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == "word1");
    REQUIRE(input == "Sequence    word2   word3.");
}

TEST_CASE("Unquoted character present in a simple string", "[unquotedCharacter]")
{
    std::string input = "This is a simple string.";
    char character = 's';
    std::set<char> quoteCharacters = { '\"', '\'' };
    REQUIRE(unquotedCharacter(input, character, quoteCharacters));
}

TEST_CASE("Quoted character should not be considered", "[unquotedCharacter]")
{
    std::string input = "This is a 'quoted' string.";
    char character = 'q';
    std::set<char> quoteCharacters = { '\"', '\'' };
    REQUIRE_FALSE(unquotedCharacter(input, character, quoteCharacters));
}

TEST_CASE("Quoted character should not be considered even if it matches", "[unquotedCharacter]")
{
    std::string input = "This is a 'q' string.";
    char character = 'q';
    std::set<char> quoteCharacters = { '\"', '\'' };
    REQUIRE_FALSE(unquotedCharacter(input, character, quoteCharacters));
}

TEST_CASE("Unquoted character not present in the string", "[unquotedCharacter]")
{
    std::string input = "This string does not contain the character.";
    char character = 'z';
    std::set<char> quoteCharacters = { '\"', '\'' };
    REQUIRE_FALSE(unquotedCharacter(input, character, quoteCharacters));
}

TEST_CASE("Unquoted character present inside quotes", "[unquotedCharacter]")
{
    std::string input = "This is a 'quoted string' with the character.";
    char character = 'c';
    std::set<char> quoteCharacters = { '\"', '\'' };
    REQUIRE_FALSE(!unquotedCharacter(input, character, quoteCharacters));
}

TEST_CASE("Unquoted character present inside nested quotes", "[unquotedCharacter]")
{
    std::string input = "Nested 'quotes with \"nested\" quotes' and the character.";
    char character = '\"';
    std::set<char> quoteCharacters = { '\"', '\'' };
    REQUIRE_FALSE(unquotedCharacter(input, character, quoteCharacters));
}
