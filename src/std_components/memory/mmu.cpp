//
// Copyright (C) 2026  HiPES - Universidade Federal do Paraná
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
 * @file mmu.cpp
 */

#include "mmu.hpp"

void MemoryManagementUnit::Clock() {
    for (int i = 0; i < this->GetNumberOfConnections(); ++i) {
        MemoryPacket packet;
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            unsigned long physicalAddress = this->TranslateAddress(packet, i);
            this->SendResponseToConnection(i, &physicalAddress);
        }
    }
}

void MemoryManagementUnit::RegisterPageTableRoot(int connId) {
    this->pageTableRoot.resize(connId + 1);
    this->pageTableRoot[connId] = ++this->lastPageTableRoot;
}

unsigned long MemoryManagementUnit::TranslateAddress(unsigned long vtAddress,
                                                      int connId) {
    if (this->pageTableRoot.size() <= (unsigned)connId) {
        this->RegisterPageTableRoot(connId);
    }
    unsigned long physAddress = 0;
    physAddress |= (this->pageTableRoot[connId] << 48) & 0xFFFF000000000000;
    physAddress |= vtAddress & 0x0000FFFFFFFFFFFF;
    return physAddress;
}

