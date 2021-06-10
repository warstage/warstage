// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__DICTIONARY_H
#define WARSTAGE__VALUE__DICTIONARY_H

#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>


class SymbolTable {
    struct Node {
        char key{};
        int next{};
        int fail{};
        int value{};
        explicit Node(char c) : key{c}, value{-1} {}
    };
    std::vector<Node> nodes_{};
    std::unordered_map<const void*, int> cache_{};
    int count_{};
    
public:
    SymbolTable();

    int FindIndex(const char* key, bool cachable) const;
    int GetIndex(const char* key, bool cachable);
    
    void Reset();
};


template <typename T>
class ValueTable {
    SymbolTable& symbols_;
    std::vector<T> values_{};
    
public:
    explicit ValueTable(SymbolTable& symbols) : symbols_{symbols} {
    }
    
    void Clear() {
        symbols_.Reset();
        values_.clear();
    }

    const std::vector<T>& Values() const {
        return values_;
    }

    const T* FindValue(const char* key) const {
        int index = symbols_.FindIndex(key, false);
        if (index == -1)
            return nullptr;
        return &values_[index];
    }
    
    T& Value(const char* key, bool cachable = false) {
        const int index = symbols_.GetIndex(key, cachable);
        while (index >= static_cast<int>(values_.size())) {
            values_.emplace_back();
        }
        return values_[index];
    }

    T& operator[](const char* key) {
        return Value(key, true);
    }

    T& operator[](const std::string& key) {
        return Value(key.c_str(), false);
    }
};

template <typename T>
class Dictionary : public SymbolTable, public ValueTable<T> {
public:
    Dictionary() : SymbolTable{}, ValueTable<T>(*static_cast<SymbolTable*>(this)) {
    }
};


#endif
