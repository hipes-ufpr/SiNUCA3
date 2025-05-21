#
# DON'T CONFIGURE THE BUILD HERE! USE config.mk INSTEAD!
#
# Generic Makefile that:
# - Autodetects C/C++ compilers to use, allowing to set defaults via cli params;
# - Autodetects all source files in the tree, compiling each one to a obj and
#   linking everything together in a final executable.
#
# Written by Gabriel de Brito (@gboncoffee everywhere) designed specifically for
# the use cases of the HiPES research group (https://web.inf.ufpr.br/hipes/).
#
# This software is dual-licensed under Public Domain (CC0) or MIT. Choose what
# fits best for you. Licenses text below.
#
# Contact mails:
# - gabrielgbrito@icloud.com (probably still exists).
# - ggb23@inf.ufpr.br (prefer this as it is my academic mail, but it may not
#   exist anymore if you're reading far enough in the future).
#
# TODO:
# - Assembly files support;
# - Better compiler autodection technology (via a configurable list maybe?);
# - Autodetection of headers associated with a source file.
#

#
# How to use and configure this build system:
#
# The targets that you should use are just target, debug and clean. The target
# target is the default and builds the "release mode". The debug builds the
# "debug mode", and clean removes the directory with objects and the binaries.
#
# Optionally set variables in the config.mk file. It works if that files doesn't
# even exist, so no worry, everything has a sane default.
#
# Variables:
#
# SRC (path): Directory with source files. Defaults to "src/".
#
# BUILD_TARGET (path): Directory with object files. Defaults to "build/".
#
# TARGET (path): Executable to create. Defaults to "a.out".
#
# DEBUG_SUFFIX (string): Will be appended to all objects and the target before
# extension, as to compile the versions with debug symbols. Defaults to
# "-debug".
#
# CPP_EXTENSION (string): File extension (without dot) to consider as C++ files,
# as some may use .cpp and some .cc. Defaults to "cpp".
#
# CC (path): Default C compiler to use. If this variable is set and the compiler
# does not exist in PATH, it autodetects between the following compilers (in
# this order): clang, gcc, cc.
#
# CPP (path): Same as CC but for C++. Autodetects between clang++, g++ and c++.
#
# FORCE_CC (bool): If this variable is set, refuses to try compiling with a
# different compiler than the default set with CC. Does nothing without CC being
# set.
#
# FORCE_CPP (bool): Same as FORCE_CC but for C++.
#
# CFLAGS (list): Default C compiler flags. Defaults to "-Wall -Wextra".
#
# C_DEBUG_FLAGS (list): Additional C compiler flags for debug build mode.
# Defaults to "-Og -g".
#
# C_RELEASE_FLAGS (list): Additional C compiler flags for release build mode.
# Defaults to "-O2".
#
# CPPFLAGS (list): Same as CFLAGS but for C++. Same defaults.
#
# CPP_DEBUG_FLAGS (list): Same as C_DEBUG_FLAGS but for C++. Same defaults.
#
# CPP_RELEASE_FLAGS (list): Same as C_RELEASE_FLAGS but for C++. Same defaults.
#
# LD_FLAGS (list): Flags to pass when linking. Defaults to none (unset).
#

#
# License option: CC0 (Public Domain)
#
# Creative Commons Legal Code
# 
# CC0 1.0 Universal
# 
#     CREATIVE COMMONS CORPORATION IS NOT A LAW FIRM AND DOES NOT PROVIDE
#     LEGAL SERVICES. DISTRIBUTION OF THIS DOCUMENT DOES NOT CREATE AN
#     ATTORNEY-CLIENT RELATIONSHIP. CREATIVE COMMONS PROVIDES THIS
#     INFORMATION ON AN "AS-IS" BASIS. CREATIVE COMMONS MAKES NO WARRANTIES
#     REGARDING THE USE OF THIS DOCUMENT OR THE INFORMATION OR WORKS
#     PROVIDED HEREUNDER, AND DISCLAIMS LIABILITY FOR DAMAGES RESULTING FROM
#     THE USE OF THIS DOCUMENT OR THE INFORMATION OR WORKS PROVIDED
#     HEREUNDER.
# 
# Statement of Purpose
# 
# The laws of most jurisdictions throughout the world automatically confer
# exclusive Copyright and Related Rights (defined below) upon the creator
# and subsequent owner(s) (each and all, an "owner") of an original work of
# authorship and/or a database (each, a "Work").
# 
# Certain owners wish to permanently relinquish those rights to a Work for
# the purpose of contributing to a commons of creative, cultural and
# scientific works ("Commons") that the public can reliably and without fear
# of later claims of infringement build upon, modify, incorporate in other
# works, reuse and redistribute as freely as possible in any form whatsoever
# and for any purposes, including without limitation commercial purposes.
# These owners may contribute to the Commons to promote the ideal of a free
# culture and the further production of creative, cultural and scientific
# works, or to gain reputation or greater distribution for their Work in
# part through the use and efforts of others.
# 
# For these and/or other purposes and motivations, and without any
# expectation of additional consideration or compensation, the person
# associating CC0 with a Work (the "Affirmer"), to the extent that he or she
# is an owner of Copyright and Related Rights in the Work, voluntarily
# elects to apply CC0 to the Work and publicly distribute the Work under its
# terms, with knowledge of his or her Copyright and Related Rights in the
# Work and the meaning and intended legal effect of CC0 on those rights.
# 
# 1. Copyright and Related Rights. A Work made available under CC0 may be
# protected by copyright and related or neighboring rights ("Copyright and
# Related Rights"). Copyright and Related Rights include, but are not
# limited to, the following:
# 
#   i. the right to reproduce, adapt, distribute, perform, display,
#      communicate, and translate a Work;
#  ii. moral rights retained by the original author(s) and/or performer(s);
# iii. publicity and privacy rights pertaining to a person's image or
#      likeness depicted in a Work;
#  iv. rights protecting against unfair competition in regards to a Work,
#      subject to the limitations in paragraph 4(a), below;
#   v. rights protecting the extraction, dissemination, use and reuse of data
#      in a Work;
#  vi. database rights (such as those arising under Directive 96/9/EC of the
#      European Parliament and of the Council of 11 March 1996 on the legal
#      protection of databases, and under any national implementation
#      thereof, including any amended or successor version of such
#      directive); and
# vii. other similar, equivalent or corresponding rights throughout the
#      world based on applicable law or treaty, and any national
#      implementations thereof.
# 
# 2. Waiver. To the greatest extent permitted by, but not in contravention
# of, applicable law, Affirmer hereby overtly, fully, permanently,
# irrevocably and unconditionally waives, abandons, and surrenders all of
# Affirmer's Copyright and Related Rights and associated claims and causes
# of action, whether now known or unknown (including existing as well as
# future claims and causes of action), in the Work (i) in all territories
# worldwide, (ii) for the maximum duration provided by applicable law or
# treaty (including future time extensions), (iii) in any current or future
# medium and for any number of copies, and (iv) for any purpose whatsoever,
# including without limitation commercial, advertising or promotional
# purposes (the "Waiver"). Affirmer makes the Waiver for the benefit of each
# member of the public at large and to the detriment of Affirmer's heirs and
# successors, fully intending that such Waiver shall not be subject to
# revocation, rescission, cancellation, termination, or any other legal or
# equitable action to disrupt the quiet enjoyment of the Work by the public
# as contemplated by Affirmer's express Statement of Purpose.
# 
# 3. Public License Fallback. Should any part of the Waiver for any reason
# be judged legally invalid or ineffective under applicable law, then the
# Waiver shall be preserved to the maximum extent permitted taking into
# account Affirmer's express Statement of Purpose. In addition, to the
# extent the Waiver is so judged Affirmer hereby grants to each affected
# person a royalty-free, non transferable, non sublicensable, non exclusive,
# irrevocable and unconditional license to exercise Affirmer's Copyright and
# Related Rights in the Work (i) in all territories worldwide, (ii) for the
# maximum duration provided by applicable law or treaty (including future
# time extensions), (iii) in any current or future medium and for any number
# of copies, and (iv) for any purpose whatsoever, including without
# limitation commercial, advertising or promotional purposes (the
# "License"). The License shall be deemed effective as of the date CC0 was
# applied by Affirmer to the Work. Should any part of the License for any
# reason be judged legally invalid or ineffective under applicable law, such
# partial invalidity or ineffectiveness shall not invalidate the remainder
# of the License, and in such case Affirmer hereby affirms that he or she
# will not (i) exercise any of his or her remaining Copyright and Related
# Rights in the Work or (ii) assert any associated claims and causes of
# action with respect to the Work, in either case contrary to Affirmer's
# express Statement of Purpose.
# 
# 4. Limitations and Disclaimers.
# 
#  a. No trademark or patent rights held by Affirmer are waived, abandoned,
#     surrendered, licensed or otherwise affected by this document.
#  b. Affirmer offers the Work as-is and makes no representations or
#     warranties of any kind concerning the Work, express, implied,
#     statutory or otherwise, including without limitation warranties of
#     title, merchantability, fitness for a particular purpose, non
#     infringement, or the absence of latent or other defects, accuracy, or
#     the present or absence of errors, whether or not discoverable, all to
#     the greatest extent permissible under applicable law.
#  c. Affirmer disclaims responsibility for clearing rights of other persons
#     that may apply to the Work or any use thereof, including without
#     limitation any person's Copyright and Related Rights in the Work.
#     Further, Affirmer disclaims responsibility for obtaining any necessary
#     consents, permissions or other rights required for any use of the
#     Work.
#  d. Affirmer understands and acknowledges that Creative Commons is not a
#     party to this document and has no duty or obligation with respect to
#     this CC0 or use of the Work.
#

#
# License option: MIT
#
# Copyright (C) 2024  Gabriel de Brito
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

undefine CC
undefine CPP

-include config.mk

# Build variables.
SRC ?= src/
BUILD_TARGET ?= build/
TARGET ?= a.out
DEBUG_SUFFIX ?= -debug

# C variables.
CFLAGS ?= -Wall -Wextra
C_DEBUG_FLAGS ?= -Og -g
C_RELEASE_FLAGS ?= -O2

# C++ variables.
CPPFLAGS ?= -Wall -Wextra
CPP_DEBUG_FLAGS ?= -Og -g
CPP_RELEASE_FLAGS ?= -O2
CPP_EXTENSION ?= cpp

#
# Compiler autodetection technology. This is particularly ugly because it looks
# like Richard Stallman doesn't know how elses work.
#

ifdef FORCE_CC
  ifndef CC
    $(error "Refusing to compile without CC being set due to FORCE_CC being set.")
  else
    ifeq '' $(shell which $(CC))
      $(error "Refusing to compile without $(CC) in PATH due to FORCE_CC being set.")
    endif
  endif
else
  ifndef CC
    CC = clang
    ifeq "$(shell which clang)" ""
      CC = gcc
      ifeq "$(shell which gcc)" ""
        CC = cc
        ifeq "$(shell which cc)" ""
          $(error "Could not detect C compiler. Set one with CC or install clang, gcc or cc.")
        endif
      endif
    endif
  endif
endif

ifdef FORCE_CPP
  ifndef CPP
    $(error "Refusing to compile without CPP being set due to FORCE_CPP being set.")
  else
    ifeq '' $(shell which $(CPP))
      $(error "Refusing to compile without $(CPP) in PATH due to FORCE_CPP being set.")
    endif
  endif
else
  ifndef CC
    CPP = clang++
    ifeq "$(shell which clang++)" ""
      CPP = g++
      ifeq "$(shell which g++)" ""
        CPP = c++
        ifeq "$(shell which c++)" ""
          $(error "Could not detect C compiler. Set one with CPP or install clang++, g++ or c++.")
        endif
      endif
    endif
  endif
endif

#
# Autodetection of source files technology.
#

C_SRCS = $(shell find $(SRC) -type f -name "*.c")
C_OBJS = $(patsubst $(SRC)%.c, $(BUILD_TARGET)%.o, $(C_SRCS))
C_DEBUG_OBJS = $(patsubst %.o, %$(DEBUG_SUFFIX).o, $(C_OBJS))

CPP_SRCS = $(shell find $(SRC) -type f -name "*.$(CPP_EXTENSION)")
CPP_OBJS = $(patsubst $(SRC)%.$(CPP_EXTENSION), $(BUILD_TARGET)%.o, $(CPP_SRCS))
CPP_DEBUG_OBJS = $(patsubst %.o, %$(DEBUG_SUFFIX).o, $(CPP_OBJS))

# Autodetect directories that need to be created. This tries to create them
# every time, if someone has a better solution please tell me.
SRC_DIRECTORIES = $(shell find $(SRC) -type d)
BUILD_TARGET_DIRECTORIES = $(patsubst $(SRC)%, $(BUILD_TARGET)%, $(SRC_DIRECTORIES))

#
# Detect if we're going to link with $(CC) or $(CPP) by simply checking if we're
# going to compile any C++ at all.
#
ifeq "$(CPP_SRCS)" ""
  LD = $(CC)
else
  LD = $(CPP)
endif

#
# Rules.
#

.PHONY: target debug clean

target: $(TARGET)

debug: $(TARGET)$(DEBUG_SUFFIX)

$(TARGET): $(CPP_OBJS) $(C_OBJS) | $(BUILD_TARGET_DIRECTORIES)
	$(LD) $^ -o $@ $(LD_FLAGS)

$(BUILD_TARGET)%.o: $(SRC)%.c | $(BUILD_TARGET_DIRECTORIES)
	$(CC) $(CFLAGS) $(C_RELEASE_FLAGS) -c $^ -o $@

$(BUILD_TARGET)%.o: $(SRC)%.$(CPP_EXTENSION) | $(BUILD_TARGET_DIRECTORIES)
	$(CPP) $(CPPFLAGS) $(CPP_RELEASE_FLAGS) -c $^ -o $@

$(TARGET)$(DEBUG_SUFFIX): $(CPP_DEBUG_OBJS) $(C_DEBUG_OBJS) | $(BUILD_TARGET_DIRECTORIES)
	$(LD) $^ -o $@ $(LD_FLAGS)

$(BUILD_TARGET)%$(DEBUG_SUFFIX).o: $(SRC)%.c | $(BUILD_TARGET_DIRECTORIES)
	$(CC) $(CFLAGS) $(C_DEBUG_FLAGS) -c $^ -o $@

$(BUILD_TARGET)%$(DEBUG_SUFFIX).o: $(SRC)%.$(CPP_EXTENSION) | $(BUILD_TARGET_DIRECTORIES)
	$(CPP) $(CPPFLAGS) $(CPP_DEBUG_FLAGS) -c $^ -o $@

$(BUILD_TARGET_DIRECTORIES):
	-mkdir -p $@

clean:
	-rm -r $(BUILD_TARGET)
	-rm $(TARGET)
	-rm $(TARGET)$(DEBUG_SUFFIX)
