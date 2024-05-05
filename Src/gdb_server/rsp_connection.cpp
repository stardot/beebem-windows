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

#include <winsock2.h>
#include <iostream>
#include <ostream>
#include <corecrt_io.h>

#include <gdb_server_utils.hpp>
#include <iomanip>
#include <rsp_connection.hpp>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>


using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::flush;
using std::hex;
using std::setfill;
using std::setw;
using namespace std;


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
  m_clientFd = -1;
}  // init ()



constexpr int DEFAULT_BUFLEN = (1024 * 1024);
static char recvbuf[DEFAULT_BUFLEN];

bool RspConnection::rspConnectWindows()
{
  constexpr char* DEFAULT_PORT = "17901";
  WSADATA wsaData;
  int iResult;

  SOCKET listenSocket = INVALID_SOCKET;
  SOCKET clientSocket = INVALID_SOCKET;

  struct addrinfo* result = NULL;
  struct addrinfo hints;

  int recvbuflen = DEFAULT_BUFLEN;

  // Initialize Winsock
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed with error: %d\n", iResult);
    return false;
  }

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  // Resolve the server address and port
  iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
  if (iResult != 0) {
    printf("getaddrinfo failed with error: %d\n", iResult);
    WSACleanup();
    return false;
  }

  // Create a SOCKET for the server to listen for client connections.
  listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (listenSocket == INVALID_SOCKET) {
    printf("socket failed with error: %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
    return false;
  }

  // Setup the TCP listening socket
  iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
  if (iResult == SOCKET_ERROR) {
    printf("bind failed with error: %d\n", WSAGetLastError());
    freeaddrinfo(result);
    closesocket(listenSocket);
    WSACleanup();
    return false;
  }

  freeaddrinfo(result);

  cout << "Listening on port " << DEFAULT_PORT << endl;

  iResult = listen(listenSocket, SOMAXCONN);
  if (iResult == SOCKET_ERROR) {
    printf("listen failed with error: %d\n", WSAGetLastError());
    closesocket(listenSocket);
    WSACleanup();
    return false;
  }

  // Accept a client socket
  clientSocket = accept(listenSocket, NULL, NULL);

  m_clientFd = (int)clientSocket;

  if (clientSocket == INVALID_SOCKET) {
    printf("accept failed with error: %d\n", WSAGetLastError());
    closesocket(listenSocket);
    WSACleanup();
    return false;
  }

  // No longer need server socket
  closesocket(listenSocket);

  return true;

}

bool RspConnection::rspConnect()
{
  if (m_useWindows) {
    return rspConnectWindows();
  }
  //else {
  //  return rspConnectLinux();
  //}
}

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
bool RspConnection::isConnected() { return -1 != m_clientFd; }  // isConnected ()


//-----------------------------------------------------------------------------
//! Close a client connection if it is open
//-----------------------------------------------------------------------------
void RspConnection::rspClose() {
  if (isConnected()) {
    std::cout << "Closing connection" << std::endl;
    close(m_clientFd);
    m_clientFd = -1;
  }
}  // rspClose ()


//-----------------------------------------------------------------------------
//! Get the next packet from the RSP connection

//! Modeled on the stub version supplied with GDB. This allows the user to
//! replace the character read function, which is why we get stuff a character
//! at at time.

//! Unlike the reference implementation, we don't deal with sequence
//! numbers. GDB has never used them, and this implementation is only intended
//! for use with GDB 6.8 or later. Sequence numbers were removed from the RSP
//! standard at GDB 5.0.

//! Since this is SystemC, if we hit something that is not a packet and
//! requires a restart/retransmission, we wait so another thread gets a lookin.

//! @param[in] pkt  The packet for storing the result.

//! @return  TRUE to indicate success, FALSE otherwise (means a communications
//!          failure)
//-----------------------------------------------------------------------------
bool RspConnection::getPkt(RspPacket* pkt) {
  // Keep getting packets, until one is found with a valid checksum
  while (true) {
    int bufSize = pkt->getBufSize();
    unsigned char checksum;  // The checksum we have computed
    int count;               // Index into the buffer
    int ch;                  // Current character

    // Wait around for the start character ('$'). Ignore all other
    // characters
    ch = getRspChar();
    while (ch != '$') {
      if (-1 == ch) {
        return false;  // Connection failed
      }
      else {
        ch = getRspChar();
      }
    }

    // Read until a '#' or end of buffer is found
    checksum = 0;
    count = 0;
    while (count < bufSize - 1) {
      ch = getRspChar();

      if (-1 == ch) {
        return false;  // Connection failed
      }

      // If we hit a start of line char begin all over again
      if ('$' == ch) {
        checksum = 0;
        count = 0;

        continue;
      }

      // Break out if we get the end of line char
      if ('#' == ch) {
        break;
      }

      // Update the checksum and add the char to the buffer
      checksum = checksum + (unsigned char)ch;
      pkt->data[count] = (char)ch;
      count++;
    }

    // Mark the end of the buffer with EOS - it's convenient for non-binary
    // data to be valid strings.
    pkt->data[count] = 0;
    pkt->setLen(count);

    // If we have a valid end of packet char, validate the checksum. If we
    // don't it's because we ran out of buffer in the previous loop.
    if ('#' == ch) {
      unsigned char xmitcsum;  // The checksum in the packet

      ch = getRspChar();
      if (-1 == ch) {
        return false;  // Connection failed
      }
      xmitcsum = GdbServerUtils::char2Hex(ch) << 4;

      ch = getRspChar();
      if (-1 == ch) {
        return false;  // Connection failed
      }

      xmitcsum += GdbServerUtils::char2Hex(ch);

      // If the checksums don't match print a warning, and put the
      // negative ack back to the client. Otherwise put a positive ack.
      if (checksum != xmitcsum) {
        cerr << "Warning: Bad RSP checksum: Computed 0x" << setw(2)
          << setfill('0') << hex << checksum << ", received 0x" << xmitcsum
          << setfill(' ') << dec << endl;
        if (!putRspChar('-'))  // Failed checksum
        {
          return false;  // Comms failure
        }
      }
      else {
        if (!putRspChar('+'))  // successful transfer
        {
          return false;  // Comms failure
        }
        else {
#ifdef RSP_TRACE
          cout << "getPkt: " << *pkt << endl;
#endif
          return true;  // Success
        }
      }
    }
    else {
      cerr << "Warning: RSP packet overran buffer" << endl;
    }
  }

}  // getPkt ()

//-----------------------------------------------------------------------------
//! Put the packet out on the RSP connection

//! Modeled on the stub version supplied with GDB. Put out the data preceded
//! by a '$', followed by a '#' and a one byte checksum. '$', '#', '*' and '}'
//! are escaped by preceding them with '}' and then XORing the character with
//! 0x20.

//! Since this is SystemC, if we hit something that requires a
//! restart/retransmission, we wait so another thread gets a lookin.

//! @param[in] pkt  The Packet to transmit

//! @return  TRUE to indicate success, FALSE otherwise (means a communications
//!          failure).
//-----------------------------------------------------------------------------
bool RspConnection::putPkt(RspPacket* pkt) {
  static char txbuf[2048];  // tx buffer
  int len = pkt->getLen();

  // Construct $<packet info>#<checksum>.
  unsigned char checksum = 0;
  txbuf[0] = '$';
  // Body of the packet
  size_t cursor = 1;
  for (size_t count = 0; count < len; count++) {
    unsigned char ch = pkt->data[count];

    // Check for escaped chars
    if (('$' == ch) || ('#' == ch) || ('*' == ch) || ('}' == ch)) {
      ch ^= 0x20;
      checksum += (unsigned char)'}';
      txbuf[cursor] = '}';
      cursor++;
    }

    checksum += ch;
    txbuf[cursor] = ch;
    cursor++;
  }

  // End char
  txbuf[cursor] = '#';
  cursor++;

  cout << "Sending Packet: " << pkt->data << std::endl;

  // Computed checksum
  txbuf[cursor] = GdbServerUtils::hex2Char(checksum >> 4);
  cursor++;
  txbuf[cursor] = GdbServerUtils::hex2Char(checksum % 16);
  cursor++;
  char ch;

  // Transmit packet
  do {  /// Repeat transmission until the GDB client ack's OK
    if (!putRspStr(txbuf, cursor)) {
      return false;  // Comms failure
    }
    // Check for ack of connection failure
    ch = getRspChar();
    if (-1 == ch) {
      return false;  // Comms failure
    }
  } while ('+' != ch);

  return true;

}  // putPkt ()


//-----------------------------------------------------------------------------
//! Get a single character from the RSP connection

//! Utility routine. This should only be called if the client is open, but we
//! check for safety.

//! @return  The character received or -1 on failure
//-----------------------------------------------------------------------------
int RspConnection::getRspChar() {
  if (-1 == m_clientFd) {
    cerr << "Warning: Attempt to read from "
      << "unopened RSP client: Ignored" << endl;
    return -1;
  }

  // Blocking read until successful (we retry after interrupts) or
  // catastrophic failure.
  while (true) {
    char c;
    int result = 0;
    if (m_useWindows) {
      result = recv(m_clientFd, &c, sizeof(c), 0);
    }
    else {
      result = read(m_clientFd, &c, sizeof(c));

    }

    switch (result) {
    case -1:
      // Error: only allow interrupts
      if (EINTR != errno) {
        cerr << "Warning: Failed to read from RSP client: "
          << "Closing client connection: " << strerror(errno) << endl;
        return -1;
      }
      break;

    case 0:
      return -1;

    default:
      return c & 0xff;  // Success, we can return (no sign extend!)
    }
  }

}  // getRspChar ()


//-----------------------------------------------------------------------------
//! Get a single character from the RSP connection

//! Utility routine. This should only be called if the client is open, but we
//! check for safety.

//! @return  The character received or -1 on failure
//-----------------------------------------------------------------------------
int RspConnection::getRspChar() {
  if (-1 == m_clientFd) {
    cerr << "Warning: Attempt to read from "
      << "unopened RSP client: Ignored" << endl;
    return -1;
  }

  // Blocking read until successful (we retry after interrupts) or
  // catastrophic failure.
  while (true) {
    char c;
    int result = 0;
    if (m_useWindows) {
      result = recv(m_clientFd, &c, sizeof(c), 0);
    }
    else {
      result = read(m_clientFd, &c, sizeof(c));

    }

    switch (result) {
    case -1:
      // Error: only allow interrupts
      if (EINTR != errno) {
        cerr << "Warning: Failed to read from RSP client: "
          << "Closing client connection: " << strerror(errno) << endl;
        return -1;
      }
      break;

    case 0:
      return -1;

    default:
      return c & 0xff;  // Success, we can return (no sign extend!)
    }
  }

}  // getRspChar ()

