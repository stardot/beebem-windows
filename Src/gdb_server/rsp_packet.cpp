// ----------------------------------------------------------------------------

// RSP packet: implementation

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

// $Id: RspPacket.cpp 327 2009-03-07 19:10:56Z jeremy $

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <rsp_packet.hpp>
#include <gdb_server_utils.hpp>
#include <iomanip>
#include <iostream>

using std::cerr;
using std::dec;
using std::endl;
using std::hex;
using std::ostream;
using std::setfill;
using std::setw;

//-----------------------------------------------------------------------------
//! Constructor

//! Allocate the new data buffer

//! @param[in]  _rspConnection  The RSP connection we will use
//! @param[in]  _bufSize        Size of data buffer to allocate
//-----------------------------------------------------------------------------
RspPacket::RspPacket(int _bufSize) : bufSize(_bufSize) {
  data = new char[_bufSize];

}  // RspPacket ();

//-----------------------------------------------------------------------------
//! Destructor

//! Give back the data buffer
//-----------------------------------------------------------------------------
RspPacket::~RspPacket() { delete[] data; }  // ~RspPacket ()

//-----------------------------------------------------------------------------
//! Pack a string into a packet.

//! A convenience version of this method.

//! @param  str  The string to copy into the data packet before sending
//-----------------------------------------------------------------------------
void RspPacket::packStr(const char* str) {
  int slen = strlen(str);

  // Construct the packet to send, so long as string is not too big, otherwise
  // truncate. Add EOS at the end for convenient debug printout
  if (slen >= bufSize) {
    cerr << "Warning: String \"" << str
      << "\" too large for RSP packet: truncated\n"
      << endl;
    slen = bufSize - 1;
  }

  strncpy(data, str, slen);
  data[slen] = 0;
  len = slen;

}  // packStr ()

//-----------------------------------------------------------------------------
//! Get the data buffer size

//! @return  The data buffer size
//-----------------------------------------------------------------------------
int RspPacket::getBufSize() { return bufSize; }  // getBufSize ()

//-----------------------------------------------------------------------------
//! Get the current number of chars in the data buffer

//! @return  The number of chars in the data buffer
//-----------------------------------------------------------------------------
int RspPacket::getLen() { return len; }  // getLen ()

//-----------------------------------------------------------------------------
//! Set the number of chars in the data buffer

//! @param[in] _len  The number of chars to be set
//-----------------------------------------------------------------------------
void RspPacket::setLen(int _len) { len = _len; }  // setLen ()

//-----------------------------------------------------------------------------
//! Output stream operator

//! @param[out] s  Stream to output to
//! @param[in]  p  Packet to output
//-----------------------------------------------------------------------------
ostream& operator<<(ostream& s, RspPacket& p) {
  return s << "RSP packet: " << std::dec << std::setw(3) << p.getLen()
    << std::setw(0) << " chars, \"" << p.data << "\"";

}  // operator<< ()
