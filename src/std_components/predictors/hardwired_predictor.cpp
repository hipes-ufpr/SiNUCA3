//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
 * @file hardwired_predictor.cpp
 * @brief Implementation of the HardwiredPredictor, a component that always
 * predicts or misses a specific set of instructions.
 */

#include "hardwired_predictor.hpp"

#include <cstring>
#include <sinuca3.hpp>

int HardwiredPredictor::Configure(Config config) {
    if (config.Bool("syscall", &this->syscall)) return 1;
    if (config.Bool("call", &this->call)) return 1;
    if (config.Bool("return", &this->ret)) return 1;
    if (config.Bool("uncond", &this->uncond)) return 1;
    if (config.Bool("cond", &this->cond)) return 1;
    if (config.Bool("noBranch", &this->noBranch)) return 1;

    if (config.ComponentReference("sendTo", &this->sendTo)) return 1;
    if (this->sendTo != NULL) this->sendToID = this->sendTo->Connect(0);

    return 0;
}

void HardwiredPredictor::Respond(int id, PredictorPacket request) {
    if (request.type == PredictorPacketTypeRequestUpdate) return;
    const InstructionPacket instruction = request.data.requestQuery;
    bool predict = true;

    // check if this is right fix
    if (instruction.staticInfo->branchType != None) {
        ++this->numberOfNoBranchs;
        predict = this->noBranch;
    } else {
        switch (instruction.staticInfo->branchType) {
            case BranchSyscall:
                predict = this->syscall;
                ++this->numberOfSyscalls;
                break;
            case BranchCall:
                predict = this->call;
                ++this->numberOfCalls;
                break;
            case BranchRet:
                predict = this->ret;
                ++this->numberOfRets;
                break;
            case BranchUncond:
                predict = this->uncond;
                ++this->numberOfUnconds;
                break;
            case BranchCond:
                predict = this->cond;
                ++this->numberOfConds;
                break;
            default: // check if this is right fix
                break;
        }
    }

    PredictorPacket response;
    response.type = PredictorPacketTypeResponseTakeToAddress;
    response.data.targetResponse.instruction = instruction;
    if (predict) {
        response.data.targetResponse.target = instruction.nextInstruction;
    } else {
        // Fast way to create an address that isn't nextInstruction.
        response.data.targetResponse.target = ~instruction.nextInstruction;
    }

    if (this->sendTo != NULL) {
        this->sendTo->SendRequest(this->sendToID, &response);
    } else {
        this->SendResponseToConnection(id, &response);
    }
}

void HardwiredPredictor::Clock() {
    const unsigned long numberOfConnections = this->GetNumberOfConnections();
    for (unsigned long i = 0; i < numberOfConnections; ++i) {
        PredictorPacket packet;
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            this->Respond(i, packet);
        }
    }
}

void HardwiredPredictor::PrintStatistics() {
    SINUCA3_LOG_PRINTF(
        "HardwiredPredictor %p: %lu syscalls executed (predict: %b).\n", this,
        this->numberOfSyscalls, this->syscall);
    SINUCA3_LOG_PRINTF(
        "HardwiredPredictor %p: %lu calls executed (predict: %b).\n", this,
        this->numberOfCalls, this->call);
    SINUCA3_LOG_PRINTF(
        "HardwiredPredictor %p: %lu rets executed (predict: %b).\n", this,
        this->numberOfRets, this->ret);
    SINUCA3_LOG_PRINTF(
        "HardwiredPredictor %p: %lu unconds executed (predict: %b).\n", this,
        this->numberOfUnconds, this->uncond);
    SINUCA3_LOG_PRINTF(
        "HardwiredPredictor %p: %lu conds executed (predict: %b).\n", this,
        this->numberOfConds, this->cond);
    SINUCA3_LOG_PRINTF(
        "HardwiredPredictor %p: %lu noBranchs executed (predict: %b).\n", this,
        this->numberOfNoBranchs, this->noBranch);
}

HardwiredPredictor::~HardwiredPredictor() {}
