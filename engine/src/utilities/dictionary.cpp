// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./dictionary.h"


SymbolTable::SymbolTable() {
    nodes_.emplace_back(0);
}


int SymbolTable::FindIndex(const char* key, bool cachable) const {
    if (cachable) {
        auto i = cache_.find(key);
        if (i != cache_.end())
            return i->second;
    }
    int index = 0;
    char c = *key++;
    while (c) {
        const Node* node = &nodes_[index];
        while (node->key != c) {
            if (node->fail) {
                index = node->fail;
                node = &nodes_[index];
            } else {
                return -1;
            }
        }
        c = *key++;
        if (c) {
            if (!node->next) {
                return -1;
            }
            index = node->next;
        }
    }
    return nodes_[index].value;
}


int SymbolTable::GetIndex(const char* key, bool cachable) {
    if (cachable) {
        auto i = cache_.find(key);
        if (i != cache_.end()) {
            assert(GetIndex(key, false) == i->second);
            return i->second;
        }
    }
    int index = 0;
    const char* k = key;
    char c = *k++;
    while (c) {
        Node* node = &nodes_[index];
        while (node->key != c) {
            if (node->fail) {
                index = node->fail;
                node = &nodes_[index];
            } else {
                node->fail = static_cast<int>(nodes_.size());
                nodes_.emplace_back(c);
                node = &nodes_[index];
            }
        }
        c = *k++;
        if (c) {
            if (!node->next) {
                node->next = static_cast<int>(nodes_.size());
                nodes_.emplace_back(c);
                node = &nodes_[index];
            }
            index = node->next;
        }
    }
    auto& result = nodes_[index];
    if (result.value == -1)
        result.value = count_++;
    if (cachable)
        cache_[key] = result.value;
    return result.value;
}


void SymbolTable::Reset() {
    nodes_.clear();
    nodes_.emplace_back(0);
    cache_.clear();
    count_ = 0;
}
