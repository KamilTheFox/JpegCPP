#ifndef HUFFMAN_MATH_H
#define HUFFMAN_MATH_H

#include <unordered_map>
#include <vector>

struct HuffmanNode {
    int symbol;
    int frequency;
    HuffmanNode* left;
    HuffmanNode* right;
    
    HuffmanNode();
    HuffmanNode(int sym, int freq);
    ~HuffmanNode();
    
    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

namespace HuffmanMath {
    HuffmanNode* buildTree(const std::unordered_map<int, int>& frequencies);
    std::unordered_map<int, std::pair<int, int>> buildCodeTable(HuffmanNode* root);
    void buildCodeTableRecursive(HuffmanNode* node, int code, int depth, 
                                std::unordered_map<int, std::pair<int, int>>& table);
}

#endif