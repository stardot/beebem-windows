// ----------------------------------------------------------------------------

// Remote Serial Protocol connection: definition

// Copyright (C) 2008  Embecosm Limited <info@embecosm.com>

// Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

// This file is part of the cycle accurate model of the OpenRISC 1000 based
// system-on-chip, ORPSoC, built using Verilator.

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

// $Id: RspConnection.h 326 2009-03-07 16:47:31Z jeremy $

#ifndef RSP_CONNECTION__H
#define RSP_CONNECTION__H

#include <rsp_packet.hpp>


//! The default service to use if port number = 0 and no service specified
#define DEFAULT_RSP_SERVICE "gdb-server123"


class RspConnection {
public:
  // Constructors and destructor
  RspConnection(int _portNum, bool useWindows) : m_useWindows(useWindows) {
    rspInit(_portNum, DEFAULT_RSP_SERVICE);
  };
  RspConnection(const char* _serviceName = DEFAULT_RSP_SERVICE);
  ~RspConnection();

  bool isConnected();

private:
  // Generic initializer
  void rspInit(int _portNum, const char* _serviceName);
  void RspConnection::rspClose();

  //! The port number to listen on
  int portNum;

  //! The service name to listen on
  const char* serviceName;

  //! The client file descriptor
  int clientFd;

  bool m_useWindows;

};  // RspConnection ()

#endif  // RSP_CONNECTION__H


