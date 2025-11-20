#include "huffman_math.h"
#include <queue>
#include <vector>
#include <stdexcept>
#include <functional>

using namespace std;

HuffmanNode::HuffmanNode() : symbol(-1), frequency(0), left(nullptr), right(nullptr) {}

HuffmanNode::HuffmanNode(int sym, int freq) : symbol(sym), frequency(freq), left(nullptr), right(nullptr) {}

HuffmanNode::~HuffmanNode() {
    delete left;
    delete right;
}

namespace HuffmanMath {
    
    struct CompareNodes {
        bool operator()(const HuffmanNode* a, const HuffmanNode* b) {
            return a->frequency > b->frequency;
        }
    };
    
    HuffmanNode* buildTree(const unordered_map<int, int>& frequencies) {
        if (frequencies.empty()) {
            throw invalid_argument("No frequencies provided");
        }
        
        priority_queue<HuffmanNode*, vector<HuffmanNode*>, CompareNodes> priorityQueue;
        
        for (const auto& kvp : frequencies) {
            auto node = new HuffmanNode(kvp.first, kvp.second);
            priorityQueue.push(node);
        }
        
        // Ð•ÑÐ»Ð¸ Ñ‚Ð¾Ð»ÑŒÐºÐ¾ Ð¾Ð´Ð¸Ð½ ÑÐ¸Ð¼Ð²Ð¾Ð» - ÑÐ¾Ð·Ð´Ð°ÐµÐ¼ Ñ„Ð¸ÐºÑ‚Ð¸Ð²Ð½Ñ‹Ð¹ ÐºÐ¾Ñ€ÐµÐ½ÑŒ
        if (priorityQueue.size() == 1) {
            auto single = priorityQueue.top();
            priorityQueue.pop();
            auto root = new HuffmanNode(-1, single->frequency);
            root->left = single;
            return root;
        }
        
        while (priorityQueue.size() > 1) {
            auto left = priorityQueue.top();
            priorityQueue.pop();
            auto right = priorityQueue.top();
            priorityQueue.pop();
            
            auto parent = new HuffmanNode(-1, left->frequency + right->frequency);
            parent->left = left;
            parent->right = right;
            
            priorityQueue.push(parent);
        }
        
        auto root = priorityQueue.top();
        priorityQueue.pop();
        return root;
    }
    
    unordered_map<int, pair<int, int>> buildCodeTable(HuffmanNode* root) {
        unordered_map<int, pair<int, int>> table;
        buildCodeTableRecursive(root, 0, 0, table);
        return table;
    }
    
    void buildCodeTableRecursive(HuffmanNode* node, int code, int depth,
                                unordered_map<int, pair<int, int>>& table) {
        if (node == nullptr) return;
        
        if (node->isLeaf()) {
            table[node->symbol] = make_pair(code, depth);
            return;
        }
        
        buildCodeTableRecursive(node->left, code << 1, depth + 1, table);
        buildCodeTableRecursive(node->right, (code << 1) | 1, depth + 1, table);
    }
}