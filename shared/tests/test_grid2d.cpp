#include <catch2/catch_test_macros.hpp>
#include <termihui/grid2d.h>
#include <string>

using namespace termihui;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("Grid2D default construction creates empty grid", "[Grid2D][construction]") {
    Grid2D<int> grid;
    
    CHECK(grid.rows() == 0);
    CHECK(grid.columns() == 0);
    CHECK(grid.size() == 0);
    CHECK(grid.empty());
}

TEST_CASE("Grid2D construction with dimensions", "[Grid2D][construction]") {
    Grid2D<int> grid(3, 4);
    
    CHECK(grid.rows() == 3);
    CHECK(grid.columns() == 4);
    CHECK(grid.size() == 12);
    CHECK_FALSE(grid.empty());
}

TEST_CASE("Grid2D construction with default value", "[Grid2D][construction]") {
    Grid2D<int> grid(2, 3, 42);
    
    CHECK(grid.rows() == 2);
    CHECK(grid.columns() == 3);
    
    // All elements should be 42
    for (size_t r = 0; r < grid.rows(); ++r) {
        for (size_t c = 0; c < grid.columns(); ++c) {
            CHECK(grid(r, c) == 42);
        }
    }
}

TEST_CASE("Grid2D construction with complex type", "[Grid2D][construction]") {
    Grid2D<std::string> grid(2, 2, "hello");
    
    CHECK(grid(0, 0) == "hello");
    CHECK(grid(0, 1) == "hello");
    CHECK(grid(1, 0) == "hello");
    CHECK(grid(1, 1) == "hello");
}

// =============================================================================
// Element Access Tests
// =============================================================================

TEST_CASE("Grid2D element access with at()", "[Grid2D][access]") {
    Grid2D<int> grid(3, 3);
    
    // Write values
    grid.at(0, 0) = 1;
    grid.at(1, 1) = 5;
    grid.at(2, 2) = 9;
    
    // Read values
    CHECK(grid.at(0, 0) == 1);
    CHECK(grid.at(1, 1) == 5);
    CHECK(grid.at(2, 2) == 9);
}

TEST_CASE("Grid2D element access with operator()", "[Grid2D][access]") {
    Grid2D<int> grid(3, 3);
    
    grid(0, 2) = 100;
    grid(2, 0) = 200;
    
    CHECK(grid(0, 2) == 100);
    CHECK(grid(2, 0) == 200);
}

TEST_CASE("Grid2D at() throws on out of bounds", "[Grid2D][access][bounds]") {
    Grid2D<int> grid(3, 4);
    
    // Valid access should not throw
    CHECK_NOTHROW(grid.at(0, 0));
    CHECK_NOTHROW(grid.at(2, 3)); // Last valid element
    
    // Out of bounds should throw
    CHECK_THROWS_AS(grid.at(3, 0), std::out_of_range);  // row out of bounds
    CHECK_THROWS_AS(grid.at(0, 4), std::out_of_range);  // column out of bounds
    CHECK_THROWS_AS(grid.at(10, 10), std::out_of_range); // both out of bounds
}

TEST_CASE("Grid2D const access", "[Grid2D][access]") {
    Grid2D<int> grid(2, 2, 7);
    
    const Grid2D<int>& constGrid = grid;
    
    CHECK(constGrid.at(0, 0) == 7);
    CHECK(constGrid(1, 1) == 7);
}

// =============================================================================
// Row Pointer Tests
// =============================================================================

TEST_CASE("Grid2D rowPointer provides direct row access", "[Grid2D][rowPointer]") {
    Grid2D<int> grid(3, 4, 0);
    
    // Fill row 1 using rowPointer
    int* row1 = grid.rowPointer(1);
    for (size_t c = 0; c < grid.columns(); ++c) {
        row1[c] = static_cast<int>(c + 10);
    }
    
    // Verify
    CHECK(grid(1, 0) == 10);
    CHECK(grid(1, 1) == 11);
    CHECK(grid(1, 2) == 12);
    CHECK(grid(1, 3) == 13);
    
    // Other rows should be unchanged
    CHECK(grid(0, 0) == 0);
    CHECK(grid(2, 0) == 0);
}

TEST_CASE("Grid2D rowPointer for contiguous memory verification", "[Grid2D][rowPointer][memory]") {
    Grid2D<int> grid(3, 4);
    
    // Fill sequentially using data pointer
    int* dataPointer = grid.data();
    for (size_t i = 0; i < grid.size(); ++i) {
        dataPointer[i] = static_cast<int>(i);
    }
    
    // Verify row-major order
    // Row 0: 0, 1, 2, 3
    // Row 1: 4, 5, 6, 7
    // Row 2: 8, 9, 10, 11
    CHECK(grid(0, 0) == 0);
    CHECK(grid(0, 3) == 3);
    CHECK(grid(1, 0) == 4);
    CHECK(grid(1, 3) == 7);
    CHECK(grid(2, 0) == 8);
    CHECK(grid(2, 3) == 11);
    
    // rowPointer should point to correct positions
    CHECK(grid.rowPointer(0) == grid.data());
    CHECK(grid.rowPointer(1) == grid.data() + 4);
    CHECK(grid.rowPointer(2) == grid.data() + 8);
}

// =============================================================================
// Fill Tests
// =============================================================================

TEST_CASE("Grid2D fill entire grid", "[Grid2D][fill]") {
    Grid2D<int> grid(3, 3, 0);
    
    grid.fill(99);
    
    for (size_t r = 0; r < grid.rows(); ++r) {
        for (size_t c = 0; c < grid.columns(); ++c) {
            CHECK(grid(r, c) == 99);
        }
    }
}

TEST_CASE("Grid2D fillRow", "[Grid2D][fill]") {
    Grid2D<int> grid(3, 4, 0);
    
    grid.fillRow(1, 55);
    
    // Row 0 unchanged
    CHECK(grid(0, 0) == 0);
    CHECK(grid(0, 3) == 0);
    
    // Row 1 filled
    CHECK(grid(1, 0) == 55);
    CHECK(grid(1, 1) == 55);
    CHECK(grid(1, 2) == 55);
    CHECK(grid(1, 3) == 55);
    
    // Row 2 unchanged
    CHECK(grid(2, 0) == 0);
    CHECK(grid(2, 3) == 0);
}

TEST_CASE("Grid2D fillRowRange", "[Grid2D][fill]") {
    Grid2D<int> grid(1, 10, 0);
    
    // Fill columns 3-7 (exclusive end)
    grid.fillRowRange(0, 3, 7, 77);
    
    CHECK(grid(0, 0) == 0);
    CHECK(grid(0, 1) == 0);
    CHECK(grid(0, 2) == 0);
    CHECK(grid(0, 3) == 77);
    CHECK(grid(0, 4) == 77);
    CHECK(grid(0, 5) == 77);
    CHECK(grid(0, 6) == 77);
    CHECK(grid(0, 7) == 0);
    CHECK(grid(0, 8) == 0);
    CHECK(grid(0, 9) == 0);
}

// =============================================================================
// Resize Tests
// =============================================================================

TEST_CASE("Grid2D resize to larger preserves data", "[Grid2D][resize]") {
    Grid2D<int> grid(2, 2);
    grid(0, 0) = 1;
    grid(0, 1) = 2;
    grid(1, 0) = 3;
    grid(1, 1) = 4;
    
    grid.resize(3, 4);
    
    CHECK(grid.rows() == 3);
    CHECK(grid.columns() == 4);
    
    // Original data preserved
    CHECK(grid(0, 0) == 1);
    CHECK(grid(0, 1) == 2);
    CHECK(grid(1, 0) == 3);
    CHECK(grid(1, 1) == 4);
    
    // New cells are default-initialized (0 for int)
    CHECK(grid(0, 2) == 0);
    CHECK(grid(0, 3) == 0);
    CHECK(grid(2, 0) == 0);
    CHECK(grid(2, 3) == 0);
}

TEST_CASE("Grid2D resize to smaller truncates data", "[Grid2D][resize]") {
    Grid2D<int> grid(3, 4);
    for (size_t r = 0; r < 3; ++r) {
        for (size_t c = 0; c < 4; ++c) {
            grid(r, c) = static_cast<int>(r * 10 + c);
        }
    }
    
    grid.resize(2, 2);
    
    CHECK(grid.rows() == 2);
    CHECK(grid.columns() == 2);
    
    // Only top-left preserved
    CHECK(grid(0, 0) == 0);
    CHECK(grid(0, 1) == 1);
    CHECK(grid(1, 0) == 10);
    CHECK(grid(1, 1) == 11);
}

TEST_CASE("Grid2D resize same size is no-op", "[Grid2D][resize]") {
    Grid2D<int> grid(2, 3, 42);
    
    grid.resize(2, 3);
    
    CHECK(grid.rows() == 2);
    CHECK(grid.columns() == 3);
    CHECK(grid(0, 0) == 42);
}

TEST_CASE("Grid2D resize with default value", "[Grid2D][resize]") {
    Grid2D<int> grid(2, 2, 0);
    grid(0, 0) = 1;
    grid(1, 1) = 4;
    
    grid.resize(3, 3, 99);
    
    // Original data preserved
    CHECK(grid(0, 0) == 1);
    CHECK(grid(1, 1) == 4);
    
    // New cells have default value
    CHECK(grid(0, 2) == 99);
    CHECK(grid(2, 0) == 99);
    CHECK(grid(2, 2) == 99);
}

TEST_CASE("Grid2D resize rows only (same columns)", "[Grid2D][resize]") {
    Grid2D<int> grid(2, 3);
    grid(0, 0) = 1;
    grid(1, 2) = 5;
    
    grid.resize(4, 3);
    
    CHECK(grid.rows() == 4);
    CHECK(grid.columns() == 3);
    CHECK(grid(0, 0) == 1);
    CHECK(grid(1, 2) == 5);
}

// =============================================================================
// Clear Tests
// =============================================================================

TEST_CASE("Grid2D clear makes grid empty", "[Grid2D][clear]") {
    Grid2D<int> grid(5, 5, 42);
    
    CHECK_FALSE(grid.empty());
    
    grid.clear();
    
    CHECK(grid.empty());
    CHECK(grid.rows() == 0);
    CHECK(grid.columns() == 0);
    CHECK(grid.size() == 0);
}

// =============================================================================
// Iterator Tests
// =============================================================================

TEST_CASE("Grid2D iterators traverse in row-major order", "[Grid2D][iterator]") {
    Grid2D<int> grid(2, 3);
    int value = 0;
    for (auto& cell : grid) {
        cell = value++;
    }
    
    // Verify row-major order
    CHECK(grid(0, 0) == 0);
    CHECK(grid(0, 1) == 1);
    CHECK(grid(0, 2) == 2);
    CHECK(grid(1, 0) == 3);
    CHECK(grid(1, 1) == 4);
    CHECK(grid(1, 2) == 5);
}

TEST_CASE("Grid2D const iterators", "[Grid2D][iterator]") {
    Grid2D<int> grid(2, 2, 7);
    const Grid2D<int>& constGrid = grid;
    
    int sum = 0;
    for (const auto& cell : constGrid) {
        sum += cell;
    }
    
    CHECK(sum == 28); // 7 * 4
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Grid2D single element grid", "[Grid2D][edge]") {
    Grid2D<int> grid(1, 1, 123);
    
    CHECK(grid.rows() == 1);
    CHECK(grid.columns() == 1);
    CHECK(grid.size() == 1);
    CHECK(grid(0, 0) == 123);
    CHECK(grid.at(0, 0) == 123);
}

TEST_CASE("Grid2D single row grid", "[Grid2D][edge]") {
    Grid2D<int> grid(1, 5);
    for (size_t c = 0; c < 5; ++c) {
        grid(0, c) = static_cast<int>(c * 2);
    }
    
    CHECK(grid(0, 0) == 0);
    CHECK(grid(0, 2) == 4);
    CHECK(grid(0, 4) == 8);
}

TEST_CASE("Grid2D single column grid", "[Grid2D][edge]") {
    Grid2D<int> grid(5, 1);
    for (size_t r = 0; r < 5; ++r) {
        grid(r, 0) = static_cast<int>(r * 3);
    }
    
    CHECK(grid(0, 0) == 0);
    CHECK(grid(2, 0) == 6);
    CHECK(grid(4, 0) == 12);
}

// =============================================================================
// Struct Cell Example (simulating terminal cell)
// =============================================================================

TEST_CASE("Grid2D with struct type", "[Grid2D][struct]") {
    struct Cell {
        char ch = ' ';
        int color = 0;
        bool bold = false;
        
        bool operator==(const Cell& other) const = default;
    };
    
    Grid2D<Cell> screen(24, 80, Cell{' ', 7, false});
    
    // Write some characters
    screen(0, 0) = Cell{'H', 1, true};
    screen(0, 1) = Cell{'i', 2, false};
    
    CHECK(screen(0, 0).ch == 'H');
    CHECK(screen(0, 0).color == 1);
    CHECK(screen(0, 0).bold == true);
    
    CHECK(screen(0, 1).ch == 'i');
    CHECK(screen(0, 1).color == 2);
    CHECK(screen(0, 1).bold == false);
    
    // Default cells
    CHECK(screen(10, 40).ch == ' ');
    CHECK(screen(10, 40).color == 7);
}
