#pragma once

#include <vector>
#include <stdexcept>
#include <cstddef>

namespace termihui {

/**
 * 2D grid container with contiguous memory layout
 * 
 * Stores elements in a single flat vector (row-major order)
 * for better cache locality compared to vector<vector<T>>
 */
template<typename T>
class Grid2D {
public:
    /**
     * Construct empty grid (0x0)
     */
    Grid2D() : rows_(0), columns_(0) {}
    
    /**
     * Construct grid with given dimensions
     * @param rows Number of rows
     * @param columns Number of columns
     */
    Grid2D(size_t rows, size_t columns)
        : rows_(rows)
        , columns_(columns)
        , data_(rows * columns)
    {}
    
    /**
     * Construct grid with given dimensions and default value
     * @param rows Number of rows
     * @param columns Number of columns
     * @param defaultValue Value to fill grid with
     */
    Grid2D(size_t rows, size_t columns, const T& defaultValue)
        : rows_(rows)
        , columns_(columns)
        , data_(rows * columns, defaultValue)
    {}
    
    /**
     * Access element at (row, column) with bounds checking
     * @throws std::out_of_range if indices are out of bounds
     */
    T& at(size_t row, size_t column) {
        checkBounds(row, column);
        return data_[row * columns_ + column];
    }
    
    /**
     * Access element at (row, column) with bounds checking (const version)
     * @throws std::out_of_range if indices are out of bounds
     */
    const T& at(size_t row, size_t column) const {
        checkBounds(row, column);
        return data_[row * columns_ + column];
    }
    
    /**
     * Access element at (row, column) without bounds checking
     * Undefined behavior if indices are out of bounds
     */
    T& operator()(size_t row, size_t column) {
        return data_[row * columns_ + column];
    }
    
    /**
     * Access element at (row, column) without bounds checking (const version)
     * Undefined behavior if indices are out of bounds
     */
    const T& operator()(size_t row, size_t column) const {
        return data_[row * columns_ + column];
    }
    
    /**
     * Get pointer to start of row (for efficient row operations)
     * @param row Row index
     * @return Pointer to first element in row
     */
    T* rowPointer(size_t row) {
        return &data_[row * columns_];
    }
    
    /**
     * Get const pointer to start of row
     */
    const T* rowPointer(size_t row) const {
        return &data_[row * columns_];
    }
    
    /**
     * Resize grid, preserving existing data where possible
     * New cells are default-initialized
     * @param newRows New number of rows
     * @param newColumns New number of columns
     */
    void resize(size_t newRows, size_t newColumns) {
        if (newRows == rows_ && newColumns == columns_) {
            return;
        }
        
        if (newColumns == columns_) {
            // Simple case: same column count, just resize
            data_.resize(newRows * newColumns);
        } else {
            // Need to reorganize data
            std::vector<T> newData(newRows * newColumns);
            
            size_t copyRows = std::min(rows_, newRows);
            size_t copyColumns = std::min(columns_, newColumns);
            
            for (size_t r = 0; r < copyRows; ++r) {
                for (size_t c = 0; c < copyColumns; ++c) {
                    newData[r * newColumns + c] = std::move(data_[r * columns_ + c]);
                }
            }
            
            data_ = std::move(newData);
        }
        
        rows_ = newRows;
        columns_ = newColumns;
    }
    
    /**
     * Resize grid with default value for new cells
     */
    void resize(size_t newRows, size_t newColumns, const T& defaultValue) {
        if (newRows == rows_ && newColumns == columns_) {
            return;
        }
        
        std::vector<T> newData(newRows * newColumns, defaultValue);
        
        size_t copyRows = std::min(rows_, newRows);
        size_t copyColumns = std::min(columns_, newColumns);
        
        for (size_t r = 0; r < copyRows; ++r) {
            for (size_t c = 0; c < copyColumns; ++c) {
                newData[r * newColumns + c] = std::move(data_[r * columns_ + c]);
            }
        }
        
        data_ = std::move(newData);
        rows_ = newRows;
        columns_ = newColumns;
    }
    
    /**
     * Fill entire grid with value
     */
    void fill(const T& value) {
        std::fill(data_.begin(), data_.end(), value);
    }
    
    /**
     * Fill specific row with value
     */
    void fillRow(size_t row, const T& value) {
        T* pointer = rowPointer(row);
        std::fill(pointer, pointer + columns_, value);
    }
    
    /**
     * Fill range in row [columnStart, columnEnd) with value
     */
    void fillRowRange(size_t row, size_t columnStart, size_t columnEnd, const T& value) {
        T* pointer = rowPointer(row);
        std::fill(pointer + columnStart, pointer + columnEnd, value);
    }
    
    /**
     * Clear grid (set to 0x0)
     */
    void clear() {
        data_.clear();
        rows_ = 0;
        columns_ = 0;
    }
    
    /**
     * Check if grid is empty
     */
    bool empty() const {
        return data_.empty();
    }
    
    /**
     * Get number of rows
     */
    size_t rows() const { return rows_; }
    
    /**
     * Get number of columns
     */
    size_t columns() const { return columns_; }
    
    /**
     * Get total number of elements
     */
    size_t size() const { return data_.size(); }
    
    /**
     * Get pointer to underlying data
     */
    T* data() { return data_.data(); }
    const T* data() const { return data_.data(); }
    
    /**
     * Iterator support (iterates in row-major order)
     */
    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }
    auto cbegin() const { return data_.cbegin(); }
    auto cend() const { return data_.cend(); }

private:
    void checkBounds(size_t row, size_t column) const {
        if (row >= rows_ || column >= columns_) {
            throw std::out_of_range(
                "Grid2D index out of range: (" + std::to_string(row) + ", " + 
                std::to_string(column) + ") in grid of size (" + 
                std::to_string(rows_) + ", " + std::to_string(columns_) + ")"
            );
        }
    }
    
    size_t rows_;
    size_t columns_;
    std::vector<T> data_;
};

} // namespace termihui
