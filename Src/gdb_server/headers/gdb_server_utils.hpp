// ----------------------------------------------------------------------------

// GDB Server Utilties: definition

// Copyright (C) 2008  Embecosm Limited <info@embecosm.com>

// Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.

// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// ----------------------------------------------------------------------------

// $Id: Utils.h 324 2009-03-07 09:42:52Z jeremy $

#pragma once

#include <cstdint>
#include <string>

//-----------------------------------------------------------------------------
//! A class offering a number of convenience utilities for the GDB Server.

//! All static functions. This class is not intended to be instantiated.
//-----------------------------------------------------------------------------
class GdbServerUtils {
public:
  static uint8_t char2Hex(int c);
  static const char hex2Char(uint8_t d);
  static void reg2Hex(uint32_t val, char* buf);
  static uint32_t hex2Reg(char* buf, size_t nBytes);
  static void ascii2Hex(char* dest, char* src);
  static void hex2Ascii(char* dest, char* src);
  static int rspUnescape(char* buf, int len);

private:
  // Private constructor cannot be instantiated
  GdbServerUtils() {};

};  // class Utils



