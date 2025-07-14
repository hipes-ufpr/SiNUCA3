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
 * @file cache_nway.cpp
 * @brief Implementation of a abstract nway cache.
 */

 // Preciso saber quantos bits serão usados como offset... ou seja... precisamos saber o tamanho de uma página.
 // Vou deixar isso aqui de exemplo antes de começar a fazer um jeito de importar essas informações de outro lugar.
 unsigned long offset_bits_mask = 12;
 unsigned long index_bits_mask = 6;
 unsigned long tag_bits_mask = 46;

 #include "cache.hpp"

#include <cassert>

#include "../../../utils/logging.hpp"

unsigned long Cache::GetIndex(unsigned long addr) const {
    return (addr & index_bits_mask) >> offset_bits_mask;
}

unsigned long Cache::GetTag(unsigned long addr) const {
    return (addr & tag_bits_mask) >> (offset_bits_mask + index_bits_mask);
}

bool Cache::GetEntry(unsigned long addr, CacheEntry **result) const{
  unsigned long tag = this->GetTag(addr);
  unsigned long index = this->GetIndex(addr);
  CacheEntry *entry;
  for_each_way(entry, index){
    if(entry->isValid && entry->tag == tag){
      *result = entry;
      return true;
    }
  }
  return false;
}

Cache::~Cache(){
    if(this->entries){
        delete[] this->entries[0];
        delete[] this->entries;
    }
}

int Cache::FinishSetup(){
  if(this->numSets == 0){
    SINUCA3_ERROR_PRINTF("TLB didn't received obrigatory parameter \"sets\"\n");
    return 1;
  }

  if(this->numWays == 0){
    SINUCA3_ERROR_PRINTF("TLB didn't received obrigatory parameter \"ways\"\n");
    return 1;
  }

  this->entries = new CacheEntry*[this->numSets];
  int n = this->numSets * this->numWays;
  assert(n > 0 && "Sanity check failed: TLB buffer size must overflowed.\n");
  this->entries[0] = new CacheEntry[n];
  memset(this->entries[0], 0, n * sizeof(CacheEntry));
  for(int i=1; i<this->numSets; ++i){
      this->entries[i] = this->entries[0] + (i * this->numWays * sizeof(CacheEntry));
  }

  for(int i=0; i<this->numSets; ++i){
      for(int j=0; j<this->numWays; ++j){
          this->entries[i][j].i = i;
          this->entries[i][j].j = j;
      }
  }

  return 0;
}

int Cache::SetConfigParameter(const char* parameter, sinuca::config::ConfigValue value)
{
  bool isSets   = (strcmp(parameter, "sets") == 0);
  bool isWays   = (strcmp(parameter, "ways") == 0);

  if(!isSets && !isWays){
    SINUCA3_ERROR_PRINTF("TLB received an unkown parameter: %s.\n", parameter);
    return 1;
  }

  if((isSets || isWays) && value.type != sinuca::config::ConfigValueTypeInteger) {
    SINUCA3_ERROR_PRINTF("TLB parameter \"%s\" is not an integer.\n",
                         isSets ? "sets" : "ways");
    return 1;
  }

  if(isSets){
    const long v = value.value.integer;
    if(v <= 0){
      SINUCA3_ERROR_PRINTF("Invalid value for TLB parameter \"sets\": should be > 0.");
      return 1;
    }
    this->numSets = v;
  }

  if(isWays){
    const long v = value.value.integer;
    if(v <= 0){
      SINUCA3_ERROR_PRINTF("Invalid value for TLB parameter \"ways\": should be > 0.");
      return 1;
    }
    this->numWays = v;
  }

  return 0;
}


void Cache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: CacheNWay Clock!\n", this);
    long numberOfConnections = this->GetNumberOfConnections();
    sinuca::MemoryPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            ++this->numberOfRequests;

            CacheEntry *result;

            // confirmar se packet está na tlb
            // se sim, responde
            if(this->Read(packet, &result)){
              this->SendResponseToConnection(i, &packet);
            } else {
              // se não,
              // chama o page-table walker (faz acesso a memoria = penalidade)
              // coloca o novo endereço na tabela de acordo com a policy.
            }
        }
    }
}

void Cache::Flush() {}

void Cache::PrintStatistics(){}
