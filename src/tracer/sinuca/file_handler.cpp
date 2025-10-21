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
 * @file file_handler.cpp
 * @details Implementation of the file handler, a helper class for handling
 * trace files.
 */

#include "file_handler.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

const int MAX_INT_DIGITS = 7;

unsigned long GetPathTidInSize(const char *sourceDir,
                                             const char *prefix,
                                             const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    /* 10 is the number of characters on the format string */
    return MAX_INT_DIGITS + 10 + sourceDirLen + prefixLen + imageNameLen;
}

void FormatPathTidIn(char *dest, const char *sourceDir,
                                   const char *prefix, const char *imageName,
                                   int tid, long destSize) {
    snprintf(dest, destSize, "%s/%s_%s_tid%u.trace", sourceDir, prefix,
             imageName, tid);
}

unsigned long GetPathTidOutSize(const char *sourceDir,
                                              const char *prefix,
                                              const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    /* 9 is the number characters in the format string */
    return 9 + sourceDirLen + prefixLen + imageNameLen;
}

void FormatPathTidOut(char *dest, const char *sourceDir,
                                    const char *prefix, const char *imageName,
                                    long destSize) {
    snprintf(dest, destSize, "%s/%s_%s.trace", sourceDir, prefix, imageName);
}
