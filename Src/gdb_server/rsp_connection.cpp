// ----------------------------------------------------------------------------

// Remote Serial Protocol connection: implementation

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

// $Id: RspConnection.cpp 327 2009-03-07 19:10:56Z jeremy $

#include <rsp_connection.hpp>
#include <iostream>
#include <winsock2.h>


//-----------------------------------------------------------------------------
//! Generic initialization routine specifying both port number and service
//! name.

//! Private, since this is not intended to be called by users. The service
//! name is only used if port number is zero.

//! Allocate the two fifos from packets from the client and to the client.

//! We only use a single packet in transit at any one time, so allocate that
//! packet here (rather than getting a new one each time.

//! @param[in] _portNum       The port number to connect to
//! @param[in] _serviceName   The service name to use (if PortNum == 0).
//-----------------------------------------------------------------------------
void RspConnection::rspInit(int _portNum, const char* _serviceName) {
  portNum = _portNum;
  serviceName = _serviceName;
  clientFd = -1;
}  // init ()

//-----------------------------------------------------------------------------
//! Destructor

//! Close the connection if it is still open
//-----------------------------------------------------------------------------
RspConnection::~RspConnection() {
  this->rspClose();  // Don't confuse with any other close ()

}  // ~RspConnection ()


//-----------------------------------------------------------------------------
//! Constructor when using a service

//! Calls the generic initializer.

//! @param[in] _serviceName  The service name to use. Defaults to
//!                          DEFAULT_RSP_SERVER
//-----------------------------------------------------------------------------
RspConnection::RspConnection(const char* _serviceName) {
  rspInit(0, _serviceName);

}  // RspConnection ()


//-----------------------------------------------------------------------------
//! Report if we are connected to a client.

//! @return  TRUE if we are connected, FALSE otherwise
//-----------------------------------------------------------------------------
bool RspConnection::isConnected() { return -1 != clientFd; }  // isConnected ()


//-----------------------------------------------------------------------------
//! Close a client connection if it is open
//-----------------------------------------------------------------------------
void RspConnection::rspClose() {
  if (isConnected()) {
    std::cout << "Closing connection" << std::endl;
    close(clientFd);
    clientFd = -1;
  }
}  // rspClose ()
