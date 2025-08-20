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

int HardwiredPredictor::FinishSetup() {
    if (this->sendTo != NULL) {
        this->sendToID = this->sendTo->Connect(0);
    }

    return 0;
}

int HardwiredPredictor::SetBoolParameter(const char* parameter, bool* ptr,
                                         ConfigValue value) {
    if (value.type != ConfigValueTypeBoolean) {
        SINUCA3_ERROR_PRINTF(
            "HardwiredPredictor parameter %s is not a boolean.\n", parameter);
        return 1;
    }

    *ptr = value.value.boolean;

    return 0;
}

int HardwiredPredictor::SetConfigParameter(const char* parameter,
                                           ConfigValue value) {
    if (strcmp("sendTo", parameter) == 0) {
        if (value.type == ConfigValueTypeComponentReference) {
            this->sendTo = dynamic_cast<Component<PredictorPacket>*>(
                value.value.componentReference);
            if (this->sendTo != NULL) return 0;
        }
        SINUCA3_ERROR_PRINTF(
            "HardwiredPredictor parameter sendTo is not a "
            "Component<PredictorPacket>.\n");
        return 1;
    }

    if (strcmp("syscall", parameter) == 0)
        return this->SetBoolParameter(parameter, &this->syscall, value);
    else if (strcmp("call", parameter) == 0)
        return this->SetBoolParameter(parameter, &this->call, value);
    else if (strcmp("return", parameter) == 0)
        return this->SetBoolParameter(parameter, &this->ret, value);
    else if (strcmp("uncond", parameter) == 0)
        return this->SetBoolParameter(parameter, &this->uncond, value);
    else if (strcmp("cond", parameter) == 0)
        return this->SetBoolParameter(parameter, &this->cond, value);
    else if (strcmp("noBranch", parameter) == 0)
        return this->SetBoolParameter(parameter, &this->noBranch, value);

    SINUCA3_ERROR_PRINTF("HardwiredPredictor received unknown parameter %s.\n",
                         parameter);
    return 1;
}

void HardwiredPredictor::Respond(int id, PredictorPacket request) {
    if (request.type == PredictorPacketTypeRequestUpdate) return;
    const InstructionPacket instruction = request.data.requestQuery;
    bool predict = true;

    if (!instruction.staticInfo->isControlFlow) {
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
            case BranchReturn:
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
        }
    }

    PredictorPacket response;
    response.type = PredictorPacketTypeResponseTakeToAddress;
    response.data.response.instruction = instruction;
    if (predict) {
        response.data.response.target = instruction.nextInstruction;
    } else {
        // Fast way to create an address that isn't nextInstruction.
        response.data.response.target = ~instruction.nextInstruction;
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
