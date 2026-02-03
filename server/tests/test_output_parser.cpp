#include <catch2/catch_test_macros.hpp>
#include "../src/OutputParser.h"

using namespace termihui;

TEST_CASE("OutputParser UTF-8 handling", "[OutputParser][utf8]") {
    OutputParser parser;
    
    SECTION("preserves UTF-8 text without ANSI codes") {
        auto segments = parser.parse("Hello мир!");
        
        REQUIRE(segments.size() == 1);
        CHECK(segments[0].text == "Hello мир!");
    }
    
    SECTION("does not treat 0x9B inside UTF-8 as CSI [regression]") {
        // Bug reproduction: Cyrillic "Л" = 0xD0 0x9B
        // 0x9B is UTF-8 continuation byte, NOT 8-bit CSI
        
        // String: "2026-02-02 - Локальные" contains "Л" (0xD0 0x9B)
        std::string input = "2026-02-02 - \xD0\x9B\xD0\xBE\xD0\xBA\xD0\xB0\xD0\xBB\xD1\x8C\xD0\xBD\xD1\x8B\xD0\xB5";
        
        auto segments = parser.parse(input);
        
        // Should be single segment, not split at 0x9B
        REQUIRE(segments.size() == 1);
        CHECK(segments[0].text == input);
        CHECK(segments[0].text.back() != '\xD0');
    }
    
    SECTION("handles Cyrillic filenames from ls output [regression]") {
        // Real data from bug: filenames with Cyrillic
        std::string input = 
            "\xD1\x81\xD0\xBA\xD1\x80\xD0\xB8\xD0\xBD\xD1\x8B\r\n"  // скрины
            "\xD0\x9B\xD0\xBE\xD0\xBA\xD0\xB0\xD0\xBB\xD1\x8C\xD0\xBD\xD1\x8B\xD0\xB5\r\n";  // Локальные
        
        auto segments = parser.parse(input);
        
        REQUIRE(segments.size() == 1);
        CHECK(segments[0].text == input);
    }
    
    SECTION("handles mixed UTF-8 and ANSI") {
        // Red "Привет" then reset
        auto segments = parser.parse("\x1B[31m\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\x1B[0m");
        
        REQUIRE(segments.size() == 1);
        CHECK(segments[0].text == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");  // Привет
        REQUIRE(segments[0].style.foreground.has_value());
    }
}

TEST_CASE("OutputParser ANSI handling", "[OutputParser][ansi]") {
    OutputParser parser;
    
    SECTION("handles 7-bit CSI SGR codes") {
        auto segments = parser.parse("normal\x1B[1mbold\x1B[0mnormal");
        
        REQUIRE(segments.size() == 3);
        CHECK(segments[0].text == "normal");
        CHECK(segments[0].style.bold == false);
        CHECK(segments[1].text == "bold");
        CHECK(segments[1].style.bold == true);
        CHECK(segments[2].text == "normal");
        CHECK(segments[2].style.bold == false);
    }
}
