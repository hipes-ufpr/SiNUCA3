//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file pseudo_lru_cache.cpp
 * @brief Implementation of a cache using PSEUDO_LRU as replacement policy.
 */

#include "pseudo_lru_cache.hpp"

#include <cassert>
#include <cstring>

#include "../../../utils/logging.hpp"
#include "cache.hpp"

PseudoLRUCache::PseudoLRUCache(){
    this->plruTree = new struct plruNode*[this->numSets];
    int n = this->numSets * (this->numWays-1);
    this->plruTree[0] = new struct plruNode[n];
    memset(this->plruTree[0], 0, n * sizeof(struct plruNode));
    for(int i=1; i<this->numSets; ++i){
        this->plruTree[i] = this->plruTree[0] + (i * this->numWays * sizeof(struct plruNode));
    }

    // Inicialize tree node's range
    int nodeCount = this->numWays-1;
    int leafNodeCount = this->numWays/2;
    int internalNodeCount = leafNodeCount - 1;
    for(int i=0; i<this->numSets; ++i){
        this->plruTree[i][0].l = 0;
        this->plruTree[i][0].r = this->numWays-1;

        struct plruNode *nodes = this->plruTree[i];
        for (int j = 0; j < internalNodeCount; ++j) {
            int mid = (nodes[j].l + nodes[j].r) / 2;

            int leftChild = 2 * j + 1;
            int rightChild = 2 * j + 2;
            if (leftChild < nodeCount) {
                nodes[leftChild].l = nodes[j].l;
                nodes[leftChild].r = mid;
            }
            if (rightChild < nodeCount) {
                nodes[rightChild].l = mid + 1;
                nodes[rightChild].r = nodes[j].r;
            }
        }
    }
}

PseudoLRUCache::~PseudoLRUCache(){
    delete[] this->plruTree[0];
    delete[] this->plruTree;
}

int PseudoLRUCache::FinishSetup(){
    if(!(this->numWays % 2)){
      SINUCA3_ERROR_PRINTF("Pseudo LRU Cache with an odd quantity of ways is not implemented yet.\n");
      return 1;
    }
    return Cache::FinishSetup();
}

bool PseudoLRUCache::Read(unsigned long addr, CacheEntry **result){
    // get entry
    int exist = GetEntry(addr, result);

    // update nodes
    int set = (*result)->i;
    int way = (*result)->j;
    int current = 0;
    while (this->plruTree[set][current].r - this->plruTree[set][current].l > 1) {
        int mid = (this->plruTree[set][current].l + this->plruTree[set][current].r) / 2;
        unsigned char direction = (way <= mid) ? 0 : 1;

        this->plruTree[set][current].direction = !direction; // marca direção oposta

        // Go to next node in the path
        current = 2 * current + 1 + direction;
    }

    // return entry
    return exist;
}

void PseudoLRUCache::Write(unsigned long addr, unsigned long value){
    unsigned long tag = this->GetTag(addr);
    unsigned long set = this->GetIndex(addr);
    int j = 0;
    while(j < this->numWays-1){
      unsigned char old_direction = this->plruTree[set][j].direction; // 0 is left, 1 is right
      this->plruTree[set][j].direction = !this->plruTree[set][j].direction; // marca direção oposta
      j = 2 * j + 1 + old_direction;
    }

    int way = j - (this->numWays-1);
    CacheEntry *plru_entry = &this->entries[set][way];
    CacheEntry new_entry = {tag, set, true, plru_entry->i, plru_entry->j, value};
    *plru_entry = new_entry;
}
