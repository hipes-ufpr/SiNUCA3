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
 * @file static_trace_writer.cpp
 * @details Implementation of StaticTraceFile class.
 */

#include "static_trace_writer.hpp"

#include <cstdlib>

#include "tracer/sinuca/file_handler.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
}

int StaticTraceWriter::OpenFile(const char* sourceDir, const char* imageName) {
    unsigned long bufferSize;
    char* path;

    bufferSize = GetPathTidOutSize(sourceDir, "static", imageName);
    path = (char*)alloca(bufferSize);
    FormatPathTidOut(path, sourceDir, "static", imageName, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc this->file\n");
        return 1;
    }
    this->header.ReserveHeaderSpace(this->file);

    return 0;
}

int StaticTraceWriter::FlushBasicBlock() {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("File pointer is nil in static trace obj!\n");
        return 1;
    }

    unsigned long occupationInBytes =
        this->basicBlockOccupation * sizeof(*basicBlock);

    if (fwrite(this->basicBlock, 1, occupationInBytes, file) !=
        occupationInBytes) {
        SINUCA3_ERROR_PRINTF("Failed to flush static records!\n");
        return 1;
    }

    return 0;
}

int StaticTraceWriter::ReallocBasicBlock() {
    this->basicBlockArraySize <<= 1;
    this->basicBlock = (StaticTraceRecord*)realloc(
        this->basicBlock,
        sizeof(StaticTraceRecord) * this->basicBlockArraySize);
    return (this->basicBlock == NULL);
}

int StaticTraceWriter::AddStaticRecord(StaticTraceRecord record, int pos) {
    if (this->IsBasicBlockArrayFull()) {
        if (this->ReallocBasicBlock()) {
            SINUCA3_ERROR_PRINTF("Failed to realloc basic block!\n");
            return 1;
        }
    }

    this->basicBlock[pos] = record;
    if (pos != 0) {
        ++this->basicBlockOccupation;
    }

    if (this->IsBasicBlockReadyToBeFlushed()) {
        if (this->FlushBasicBlock()) {
            SINUCA3_ERROR_PRINTF("Failed to flush basic block!\n");
            return 1;
        }
        this->ResetBasicBlock();
    }

    return 0;
}

int StaticTraceWriter::AddBasicBlockSize(unsigned int basicBlockSize) {
    if (!this->WasBasicBlockReset()) {
        SINUCA3_ERROR_PRINTF("Basic block control variables were not reset!\n");
        return 1;
    }
    if (basicBlockSize == 0) {
        SINUCA3_ERROR_PRINTF("Basic block size is not expected to be 0!\n");
        return 1;
    }

    StaticTraceRecord record;

    record.recordType = StaticRecordBasicBlockSize;
    record.data.basicBlockSize = basicBlockSize;
    this->currentBasicBlockSize = basicBlockSize;

    return (this->AddStaticRecord(record, 0));
}

int StaticTraceWriter::AddInstruction(const Instruction* inst) {
    if (inst == NULL) {
        SINUCA3_ERROR_PRINTF("Instruction pointer is nil!\n");
        return 1;
    }

    StaticTraceRecord record;

    record.recordType = StaticRecordInstruction;
    record.data.instruction = *inst;

    return (this->AddStaticRecord(record, this->basicBlockOccupation));
}
