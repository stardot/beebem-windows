// ----------------------------------------------------------------------------

// SystemC GDB RSP server: implementation

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

// $Id: GdbServerSC.cpp 331 2009-03-12 17:01:48Z jeremy $

#include <chrono>
#include <gdb_server.hpp>
#include <i_simulation_control.hpp>
#include <rsp_connection.hpp>
#include <rsp_packet.hpp>
#include <gdb_server_utils.hpp>
#include <iomanip>
#include <iostream>
#include <thread>

using std::cerr;
using std::dec;
using std::endl;
using std::hex;

GdbServer::GdbServer(ISimulationControl* simCtrl, int rspPort)
  : m_simCtrl(simCtrl) {
  pkt = new RspPacket(RSP_PKT_MAX);
  rsp = new RspConnection(rspPort, true);
}  // GdbServer ()

GdbServer::~GdbServer() {
  delete rsp;
  delete pkt;

}  // ~GdbServer

//-----------------------------------------------------------------------------
//! Thread to listen for RSP requests and control target
//-----------------------------------------------------------------------------
void GdbServer::serverThread() {
  m_simCtrl->setServerRunning(true);
  // Loop processing commands forever
  while (!m_simCtrl->shouldStopServer()) {
    // Make sure we are still connected.
    while (!rsp->isConnected() && !m_simCtrl->shouldStopServer()) {
      // Reconnect and stall the processor on a new connection
      if (!rsp->rspConnectWindows()) {
        // Serious failure. Must abort execution.
        cerr << "*** Unable to continue: ABORTING" << endl;
        exit(1);
      }

      // Stall the processor until we get a command to handle.
      if (!m_simCtrl->isStalled()) {
        m_simCtrl->stall();
      }

      targetStopped = true;  // Processor now not running
    }

    while (!targetStopped && !m_simCtrl->shouldStopServer()) {
      if (m_simCtrl->isStalled()) {
        targetStopped = true;

        // Tell the client we've stopped.
        rspReportException();
      }
      // Wait while target is running
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Get a RSP client request
    if (!m_simCtrl->shouldStopServer()) {
      rspClientRequest();
    }
  }
  m_simCtrl->setServerRunning(false);
}  // rspServer ()

//-----------------------------------------------------------------------------
//! Deal with a request from the GDB client session

//! In general, apart from the simplest requests, this function replies on
//! other functions to implement the functionality.

//! @note It is the responsibility of the recipient to delete the packet when
//!       it is finished with. It is permissible to reuse the packet for a
//!       reply.

//! @todo Is this the implementation of the 'D' packet really the intended
//!       meaning? Or does it just mean that only vAttach will be recognized
//!       after this?

//! @param[in] pkt  The received RSP packet
//-----------------------------------------------------------------------------
void GdbServer::rspClientRequest() {
  if (!rsp->getPkt(pkt)) {
    rsp->rspClose();  // Comms failure
    return;
  }

  switch (pkt->data[0]) {
  case '!':
    // Request for extended remote mode
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;

  case '?':
    // Return last signal ID
    rspReportException();
    return;

  case 'A':
    // Initialization of argv not supported
    cerr << "Warning: RSP 'A' packet not supported: ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;

  case 'b':
    // Setting baud rate is deprecated
    cerr << "Warning: RSP 'b' packet is deprecated and not "
      << "supported: ignored" << endl;
    return;

  case 'B':
    // Breakpoints should be set using Z packets
    cerr << "Warning: RSP 'B' packet is deprecated (use 'Z'/'z' "
      << "packets instead): ignored" << endl;
    return;

  case 'c':
    // Continue
    rspContinue(EXCEPT_NONE);
    return;

  case 'C':
    // Continue with signal (in the packet)
    rspContinue();
    return;

  case 'd':
    // Disable debug using a general query
    cerr << "Warning: RSP 'd' packet is deprecated (define a 'Q' "
      << "packet instead: ignored" << endl;
    return;

  case 'D':
    // Detach GDB. Do this by closing the client. The rules say that
    // execution should continue, so unstall the processor.
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    m_simCtrl->unstall();
    targetStopped = false;
    return;

  case 'F':
    // File I/O is not currently supported
    cerr << "Warning: RSP file I/O not currently supported: 'F' "
      << "packet ignored" << endl;
    return;

  case 'g':
    rspReadAllRegs();
    return;

  case 'G':
    rspWriteAllRegs();
    return;

  case 'H':
    // Set the thread number of subsequent operations.
    // silently and just reply "OK"
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;

  case 'i':
  case 'I':
    // Single cycle step not currently supported. Mark the target as
    // running, so that next time it will be detected as stopped (it is
    // still stalled in reality) and an ack sent back to the client.
    cerr << "Warning: RSP cycle stepping not supported: target "
      << "stopped immediately" << endl;
    targetStopped = false;
    return;

  case 'k':
    // Kill request. Stop simulation
    // spdlog::info("Simulation stopped by gdb client.");
    m_simCtrl->stopServer();
    m_simCtrl->kill();
    return;

  case 'm':
    // Read memory (symbolic)
    rspReadMem();
    return;

  case 'M':
    // Write memory (symbolic)
    rspWriteMem();
    return;

  case 'p':
    // Read a register
    rspReadReg();
    return;

  case 'P':
    // Write a register
    rspWriteReg();
    return;

  case 'q':
    // Any one of a number of query packets
    rspQuery();
    return;

  case 'Q':
    // Any one of a number of set packets
    rspSet();
    return;

  case 'r':
    // Reset the system. Deprecated (use 'R' instead)
    cerr << "Warning: RSP 'r' packet is deprecated (use 'R' "
      << "packet instead): ignored" << endl;
    return;

  case 'R':
    // Restart the program being debugged.
    rspRestart();
    return;

  case 's':
    // Single step one machine instruction.
    rspStep(EXCEPT_NONE);
    return;

  case 'S':
    // Single step one machine instruction.
    rspStep();
    return;

  case 't':
    // Search. This is not well defined in the manual and for now we don't
    // support it. No response is defined.
    cerr << "Warning: RSP 't' packet not supported: ignored" << endl;
    return;

  case 'T':
    // Is the thread alive. We are bare metal, so don't have a thread
    // context. The answer is always "OK".
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;

  case 'v':
    // Any one of a number of packets to control execution
    rspVpkt();
    return;

  case 'X':
    // Write memory (binary)
    rspWriteMemBin();
    return;

  case 'z':
    // Remove a breakpoint/watchpoint.
    rspRemoveMatchpoint();
    return;

  case 'Z':
    // Insert a breakpoint/watchpoint.
    rspInsertMatchpoint();
    return;

  default:
    // Unknown commands are ignored
    cerr << "Warning: Unknown RSP request" << pkt->data << endl;
    return;
  }
}  // rspClientRequest ()



//-----------------------------------------------------------------------------
//! Send a packet acknowledging an exception has occurred
//! The only signal we ever see in this implementation is TRAP.
//-----------------------------------------------------------------------------
void GdbServer::rspReportException() {
  // Construct a signal received packet
  pkt->data[0] = 'S';
  pkt->data[1] = GdbServerUtils::hex2Char(TARGET_SIGNAL_TRAP >> 4);
  pkt->data[2] = GdbServerUtils::hex2Char(TARGET_SIGNAL_TRAP % 16);
  pkt->data[3] = '\0';
  pkt->setLen(strlen(pkt->data));

  rsp->putPkt(pkt);

}  // rspReportException ()


//-----------------------------------------------------------------------------
//! Handle a RSP continue request

//! This version is typically used for the 'c' packet, to continue without
//! signal, in which case EXCEPT_NONE is passed in as the exception to use.

//! At present other exceptions are not supported

//! @param[in] except  The exception to use
//-----------------------------------------------------------------------------
void GdbServer::rspContinue(uint32_t except) {
  uint32_t addr;  // Address to continue from, if any

  // Reject all except 'c' packets
  if ('c' != pkt->data[0]) {
    cerr << "Warning: Continue with signal not currently supported: "
      << "ignored" << endl;
    return;
  }

  // Get an address if we have one
  if (0 == strcmp("c", pkt->data)) {
    // Proceed
  }
  else if (1 != sscanf(pkt->data, "c%x", &addr)) {
    cerr << "Warning: RSP continue address " << pkt->data
      << " not recognized: ignored" << endl;
  }
  addr = m_simCtrl->readReg(m_simCtrl->pcRegNum());  // Default uses current PC

  rspContinue(addr, EXCEPT_NONE);

}  // rspContinue (uint32_t except)


//-----------------------------------------------------------------------------
//! Handle a RSP continue with signal request

//! @todo Currently does nothing. Will use the underlying generic continue
//!       function.
//-----------------------------------------------------------------------------
void GdbServer::rspContinue() {
  cerr << "RSP continue with signal '" << pkt->data << "' received" << endl;

}  // rspContinue ()

//-----------------------------------------------------------------------------
//! Generic processing of a continue request

//! The signal may be EXCEPT_NONE if there is no exception to be
//! handled. Currently the exception is ignored.

//! The single step flag is cleared in the debug registers and then the
//! processor is unstalled.

//! @param[in] addr    Address from which to step
//! @param[in] except  The exception to use (if any)
//-----------------------------------------------------------------------------
void GdbServer::rspContinue(uint32_t addr, uint32_t except) {
  m_simCtrl->unstall();
  targetStopped = false;
}  // rspContinue (uint32_t addr, uint32_t except)



//-----------------------------------------------------------------------------
//! Handle a RSP read all registers request

//! Each register is returned as a sequence of bytes in target endian order.

//! Each byte is packed as a pair of hex digits.
//-----------------------------------------------------------------------------
void GdbServer::rspReadAllRegs() {
  for (int r = 0; r < m_simCtrl->nRegs(); r++) {
    GdbServerUtils::reg2Hex(m_simCtrl->htotl(m_simCtrl->readReg(r)),
      &(pkt->data[r * 8]));
  }
  pkt->data[m_simCtrl->nRegs() * 8] = 0;
  pkt->setLen(m_simCtrl->nRegs() * 8);
  rsp->putPkt(pkt);

}  // rspReadAllRegs ()



//-----------------------------------------------------------------------------
//! Handle a RSP write all registers request
//! Each register is supplied as a sequence of bytes in target endian order.
//! Each byte is packed as a pair of hex digits.
//! @todo There is no error checking at present. Non-hex chars will generate a
//!       warning message, but there is no other check that the right amount
//!       of data is present. The result is always "OK".
//-----------------------------------------------------------------------------
void GdbServer::rspWriteAllRegs() {
  for (int r = 0; r < m_simCtrl->nRegs(); r++) {
    m_simCtrl->writeReg(
      r, GdbServerUtils::hex2Reg(&(pkt->data[r * 8]), m_simCtrl->wordSize()));
  }

  // Acknowledge (always OK for now).
  pkt->packStr("OK");
  rsp->putPkt(pkt);

}  // rspWriteAllRegs ()



//-----------------------------------------------------------------------------
//! Handle a RSP read memory (symbolic) request

//! Syntax is:

//!   m<addr>,<length>:

//! The response is the bytes, lowest address first, encoded as pairs of hex
//! digits.

  // Read memory from device
constexpr int MAX_SIZE_WE_CAN_DO = (1024 * 1024) * 20;
static uint8_t rawMem[MAX_SIZE_WE_CAN_DO];

//! The length given is the number of bytes to be read.
//-----------------------------------------------------------------------------
void GdbServer::rspReadMem() {
  unsigned int addr;  // Where to read the memory
  int len;            // Number of bytes to read
  int off;            // Offset into the memory


  if (2 != sscanf(pkt->data, "m%x,%x:", &addr, &len)) {
    cerr << "Warning: Failed to recognize RSP read memory command: "
      << pkt->data << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  // Make sure we won't overflow the buffer (2 chars per byte)
  if ((len * 2) >= pkt->getBufSize()) {
    cerr << "Warning: Memory read " << pkt->data
      << " too large for RSP packet: truncated" << endl;
    len = (pkt->getBufSize() - 1) / 2;
  }

  m_simCtrl->readMem(rawMem, addr, len);

  // Convert to hex string
  for (int i = 0; i < len; i++) {
    unsigned char ch = rawMem[i];
    pkt->data[i * 2] = GdbServerUtils::hex2Char(ch >> 4);
    pkt->data[i * 2 + 1] = GdbServerUtils::hex2Char(ch & 0xf);
  }

  pkt->data[len * 2] = '\0';  // End of string
  pkt->setLen(strlen(pkt->data));
  rsp->putPkt(pkt);

}  // rsp_read_mem ()

//-----------------------------------------------------------------------------
//! Handle a RSP write memory (symbolic) request

//! Syntax is:

//!   m<addr>,<length>:<data>

//! The data is the bytes, lowest address first, encoded as pairs of hex
//! digits.

//! The length given is the number of bytes to be written.
//-----------------------------------------------------------------------------
void GdbServer::rspWriteMem() {
  uint32_t addr;  // Where to write the memory
  int len;        // Number of bytes to write

  if (2 != sscanf(pkt->data, "M%x,%x:", &addr, &len)) {
    cerr << "Warning: Failed to recognize RSP write memory " << pkt->data
      << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  // Find the start of the data and check there is the amount we expect.
  char* symDat = (char*)(memchr(pkt->data, ':', pkt->getBufSize())) + 1;
  int datLen = pkt->getLen() - (symDat - pkt->data);

  // Sanity check
  if (len * 2 != datLen) {
    cerr << "Warning: Write of " << len * 2 << "digits requested, but "
      << datLen << " digits supplied: packet ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  // Write the bytes to memory (no check the address is OK here)
  for (int off = 0; off < len; off++) {
    uint8_t nyb1 = GdbServerUtils::char2Hex(symDat[off * 2]);
    uint8_t nyb2 = GdbServerUtils::char2Hex(symDat[off * 2 + 1]);
    unsigned char c = (nyb1 << 4) | nyb2;
    m_simCtrl->writeMem(&c, addr + off, 1);
  }

  pkt->packStr("OK");
  rsp->putPkt(pkt);

}  // rspWriteMem ()



//-----------------------------------------------------------------------------
//! Read a single register
//! The register is returned as a sequence of bytes in target endian order.
//! Each byte is packed as a pair of hex digits.
//-----------------------------------------------------------------------------
void GdbServer::rspReadReg() {
  unsigned int regNum;

  // Break out the fields from the data
  if (1 != sscanf(pkt->data, "p%x", &regNum)) {
    cerr << "Warning: Failed to recognize RSP read register command: "
      << pkt->data << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  GdbServerUtils::reg2Hex(m_simCtrl->htotl(m_simCtrl->readReg(regNum)), pkt->data);
  pkt->setLen(strlen(pkt->data));
  rsp->putPkt(pkt);

}  // rspWriteReg ()

//-----------------------------------------------------------------------------
//! Write a single register
//! The register is specified as a sequence of bytes in target endian order.
//! Each byte is packed as a pair of hex digits.
//-----------------------------------------------------------------------------
void GdbServer::rspWriteReg() {
  unsigned int regNum;
  char valstr[9];  // Allow for EOS on the string

  // Break out the fields from the data
  if (2 != sscanf(pkt->data, "P%x=%8s", &regNum, valstr)) {
    cerr << "Warning: Failed to recognize RSP write register command "
      << pkt->data << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  m_simCtrl->writeReg(regNum, GdbServerUtils::hex2Reg(valstr, m_simCtrl->wordSize()));
  pkt->packStr("OK");
  rsp->putPkt(pkt);

}  // rspWriteReg ()



//-----------------------------------------------------------------------------
//! Handle a RSP query request
//-----------------------------------------------------------------------------
void GdbServer::rspQuery() {
  if (0 == strcmp("qC", pkt->data)) {
    // Return the current thread ID (unsigned hex). A null response
    // indicates to use the previously selected thread.
    sprintf(pkt->data, "QC%x", 0);
    pkt->setLen(strlen(pkt->data));
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qCRC", pkt->data, strlen("qCRC"))) {
    // Return CRC of memory area
    cerr << "Warning: RSP CRC query not supported" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
  }
  else if (0 == strcmp("qfThreadInfo", pkt->data)) {
    // Return info about active threads. We return just the constant
    sprintf(pkt->data, "m%x", 0);
    pkt->setLen(strlen(pkt->data));
    rsp->putPkt(pkt);
  }
  else if (0 == strcmp("qsThreadInfo", pkt->data)) {
    // Return info about more active threads. We have no more, so return the
    // end of list marker, 'l'
    pkt->packStr("l");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qGetTLSAddr:", pkt->data, strlen("qGetTLSAddr:"))) {
    // We don't support this feature
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qL", pkt->data, strlen("qL"))) {
    // Deprecated and replaced by 'qfThreadInfo'
    cerr << "Warning: RSP qL deprecated: no info returned" << endl;
    pkt->packStr("qM001");
    rsp->putPkt(pkt);
  }
  else if (0 == strcmp("qOffsets", pkt->data)) {
    // Report any relocation
    //        pkt->packStr ("Text=0;Data=0;Bss=0");
    pkt->packStr("");  // Not supported
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qP", pkt->data, strlen("qP"))) {
    // Deprecated and replaced by 'qThreadExtraInfo'
    cerr << "Warning: RSP qP deprecated: no info returned" << endl;
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qRcmd,", pkt->data, strlen("qRcmd,"))) {
    // "Passed to the local interpreter for execution"
    cerr << "Warning: RSP qRcmd not supported." << endl;
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qSupported", pkt->data, strlen("qSupported"))) {
    // Report a list of the features we support. For now we just ignore any
    // supplied specific feature queries, but in the future these may be
    // supported as well. Note that the packet size allows for 'G' + all the
    // registers sent to us, or a reply to 'g' with all the registers and an
    // EOS so the buffer is a well formed string.
    sprintf(pkt->data, "PacketSize=%x", pkt->getBufSize());
    pkt->setLen(strlen(pkt->data));
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qSymbol:", pkt->data, strlen("qSymbol:"))) {
    // Offer to look up symbols. Nothing we want (for now). TODO. This just
    // ignores any replies to symbols we looked up, but we didn't want to
    // do that anyway!
    pkt->packStr("OK");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qThreadExtraInfo,", pkt->data,
    strlen("qThreadExtraInfo,"))) {
    // Report that we are runnable, but the text must be hex ASCI
    // digits. For now do this by steam, reusing the original packet
    sprintf(pkt->data, "%02x%02x%02x%02x%02x%02x%02x%02x%02x", 'R', 'u', 'n',
      'n', 'a', 'b', 'l', 'e', 0);
    pkt->setLen(strlen(pkt->data));
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qXfer:", pkt->data, strlen("qXfer:"))) {
    // For now we support no 'qXfer' requests, but these should not be
    // expected, since they were not reported by 'qSupported'
    cerr << "Warning: RSP 'qXfer' not supported: ignored" << endl;
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qAttached", pkt->data, strlen("qAttached"))) {
    // Client asks if a new process was created, or if we attached to an
    // existing one.
    pkt->packStr("1");  // existing process
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qTfV", pkt->data, strlen("qTfV"))) {
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qTfP", pkt->data, strlen("qTfP"))) {
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("qTStatus", pkt->data, strlen("qTStatus"))) {
    // Client asks if there is a trace experiment running right now.
    // Reply that this packet is unsupported.
    //        pkt->packStr("Ttnotrun:0");
    //        pkt->packStr("tunknown:0");

    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else {
    cerr << "Unrecognized RSP query: ignored" << endl;
  }
}  // rspQuery ()



//-----------------------------------------------------------------------------
//! Handle a qSupported? feature query
//-----------------------------------------------------------------------------
void GdbServer::qSupported() {
  char* query = pkt->data + strlen("qSupported:");
  char* response = new char[pkt->getLen()];
  size_t responseLen;
  size_t totLen = 0;

  // Process every feature request
  // '[feature]-' Means feature not supported
  // '[feature]+' Means feature is supported
  char* queryEnd = pkt->data + pkt->getLen();
  while (query < queryEnd) {
    if (0 == strncmp("multiprocess+", query, strlen("multiprocess+"))) {
      query += strlen("multiprocess+");

      responseLen = sprintf(response, "multiprocess-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 == strncmp("swbreak+", query, strlen("swbreak+"))) {
      query += strlen("swbreak+");

      responseLen = sprintf(response, "swbreak-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 == strncmp("hwbreak+", query, strlen("hwbreak+"))) {
      query += strlen("hwbreak+");

      responseLen = sprintf(response, "hwbreak+;");  // Supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 == strncmp("qRelocInsn+", query, strlen("qRelocInsn+"))) {
      query += strlen("qRelocInsn+");

      responseLen = sprintf(response, "qRelocInsn-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 == strncmp("fork-events+", query, strlen("fork-events+"))) {
      query += strlen("fork-events+");

      responseLen = sprintf(response, "fork-events-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 == strncmp("vfork-events+", query, strlen("vfork-events+"))) {
      query += strlen("vfork-events+");

      responseLen = sprintf(response, "vfork-events-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 == strncmp("exec-events+", query, strlen("exec-events+"))) {
      query += strlen("exec-events+");

      responseLen = sprintf(response, "vfork-events-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 ==
      strncmp("vContSupported+", query, strlen("vContSupported+"))) {
      query += strlen("vContSupported+");

      responseLen = sprintf(response, "vContSupported+;");  // Supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 ==
      strncmp("QThreadEvents+", query, strlen("QThreadEvents+"))) {
      query += strlen("QThreadEvents+");

      responseLen = sprintf(response, "QThreadEvents-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }
    else if (0 == strncmp("no-resumed+", query, strlen("no-resumed+"))) {
      query += strlen("no-resumed+");

      responseLen = sprintf(response, "no-resumed-;");  // Not supported
      response += responseLen;
      totLen += responseLen;
    }

    // Ignore any unrecognised queries

    if (*query == ';') {
      // Proceed to next command
      query++;
    }
  }

  // Null-terminate response
  *response = 0;

  // Reset response pointer
  response -= totLen;

  // Transmit packet
  pkt->packStr(response);  // Pack response string into packet for tx
  rsp->putPkt(pkt);

  // clean-up
  delete[](response);

}  // qSupported ()


//-----------------------------------------------------------------------------
//! Handle a RSP set request
//-----------------------------------------------------------------------------
void GdbServer::rspSet() {
  if (0 == strncmp("QPassSignals:", pkt->data, strlen("QPassSignals:"))) {
    // Passing signals not supported
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else if ((0 == strncmp("QTDP", pkt->data, strlen("QTDP"))) ||
    (0 == strncmp("QFrame", pkt->data, strlen("QFrame"))) ||
    (0 == strcmp("QTStart", pkt->data)) ||
    (0 == strcmp("QTStop", pkt->data)) ||
    (0 == strcmp("QTinit", pkt->data)) ||
    (0 == strncmp("QTro", pkt->data, strlen("QTro"))) ||
    (0 == strncmp("QStartNoAckMode", pkt->data, strlen("QStartNoAckMode")))
    ) {
    // All tracepoint features are not supported. This reply is really only
    // needed to 'QTDP', since with that the others should not be
    // generated.
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else {
    cerr << "Unrecognized RSP set request: ignored" << endl;
    delete pkt;
  }
}  // rspSet ()

//-----------------------------------------------------------------------------
//! Handle a RSP restart request

//! not implemented
//-----------------------------------------------------------------------------
void GdbServer::rspRestart() {
  // spdlog::error("GdbServer.cpp: RSP restart request not implemented.");
}  // rspRestart ()

//-----------------------------------------------------------------------------
//! Handle a RSP step request

//! This version is typically used for the 's' packet, to continue without
//! signal, in which case EXCEPT_NONE is passed in as the exception to use.

//! @param[in] except  The exception to use. Only EXCEPT_NONE should be set
//!                    this way.
//-----------------------------------------------------------------------------
void GdbServer::rspStep(uint32_t except) {
  uint32_t addr;  // The address to step from, if any

  // Reject all except 's' packets
  if ('s' != pkt->data[0]) {
    cerr << "Warning: Step with signal not currently supported: "
      << "ignored" << endl;
    return;
  }

  if (0 == strcmp("s", pkt->data)) {
    // No address supplied, use PC
  }
  else if (1 != sscanf(pkt->data, "s%x", &addr)) {
    cerr << "Warning: RSP step address " << pkt->data << " not ignored" << endl;
    // Still just use PC
  }
  addr = m_simCtrl->readReg(m_simCtrl->pcRegNum());

  rspStep(addr, EXCEPT_NONE);

}  // rspStep ()

//-----------------------------------------------------------------------------
//! Handle a RSP step with signal request

//! @todo Not implemented
//-----------------------------------------------------------------------------
void GdbServer::rspStep() {
  // spdlog::error("GdbServer.cpp: rspStep not implemented");
}  // rspStep ()

//-----------------------------------------------------------------------------
//! Generic processing of a step request

//! The signal may be EXCEPT_NONE if there is no exception to be
//! handled. Currently the exception is ignored.
//! @param[in] addr    Address from which to step
//! @param[in] except  The exception to use (if any)
//-----------------------------------------------------------------------------
void GdbServer::rspStep(uint32_t addr, uint32_t except) {
  // Set the address as the value of the next program counter
  m_simCtrl->writeReg(m_simCtrl->pcRegNum(), addr);
  m_simCtrl->step();
  targetStopped = false;
}  // rspStep ()

//-----------------------------------------------------------------------------
//! Handle a RSP 'v' packet

//! These are commands associated with executing the code on the target
//-----------------------------------------------------------------------------
void GdbServer::rspVpkt() {
  if (0 == strncmp("vAttach;", pkt->data, strlen("vAttach;"))) {
    // Attaching is a null action, since we have no other process. We just
    // return a stop packet (using TRAP) to indicate we are stopped.
    pkt->packStr("S05");
    rsp->putPkt(pkt);
    return;
  }
  else if (0 == strcmp("vCont?", pkt->data)) {
    // For now we don't support this.
    pkt->packStr("vCont;s;c");
    rsp->putPkt(pkt);
    return;
  }
  else if (0 == strncmp("vCont", pkt->data, strlen("vCont"))) {
    // This shouldn't happen, because we've reported non-support via vCont?
    // above
    cerr << "Warning: RSP vCont not supported: ignored" << endl;
    return;
  }
  else if (0 == strncmp("vFile:", pkt->data, strlen("vFile:"))) {
    // For now we don't support this.
    cerr << "Warning: RSP vFile not supported: ignored" << endl;
    pkt->packStr("");
    rsp->putPkt(pkt);
    return;
  }
  else if (0 == strncmp("vFlashErase:", pkt->data, strlen("vFlashErase:"))) {
    // For now we don't support this.
    cerr << "Warning: RSP vFlashErase not supported: ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }
  else if (0 == strncmp("vFlashWrite:", pkt->data, strlen("vFlashWrite:"))) {
    // For now we don't support this.
    cerr << "Warning: RSP vFlashWrite not supported: ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }
  else if (0 == strcmp("vFlashDone", pkt->data)) {
    // For now we don't support this.
    cerr << "Warning: RSP vFlashDone not supported: ignored" << endl;
    ;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }
  else if (0 == strncmp("vRun;", pkt->data, strlen("vRun;"))) {
    // We shouldn't be given any args, but check for this
    if (pkt->getLen() > strlen("vRun;")) {
      cerr << "Warning: Unexpected arguments to RSP vRun "
        "command: ignored"
        << endl;
    }

    // Restart the current program. However unlike a "R" packet, "vRun"
    // should behave as though it has just stopped. We use signal 5 (TRAP).
    rspRestart();
    pkt->packStr("S05");
    rsp->putPkt(pkt);
  }
  else if (0 == strncmp("vKill", pkt->data, strlen("vKill"))) {
    // Kill request - stop simulation
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    // spdlog::info("Simulation stopped by gdb client");
    m_simCtrl->kill();
    m_simCtrl->stopServer();
  }
  else if (0 ==
    strncmp("vMustReplyEmpty", pkt->data, strlen("vMustReplyEmpty"))) {
    // Reply empty packet
    pkt->packStr("");
    rsp->putPkt(pkt);
  }
  else {
    // spdlog::warn("GdbServer: Unknown RSP 'v' packet type {:s} ignored.",pkt->data);
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }
}  // rspVpkt ()

//-----------------------------------------------------------------------------
//! Handle a RSP write memory (binary) request

//! Syntax is:

//!   X<addr>,<length>:

//! Followed by the specified number of bytes as raw binary. Response should be
//! "OK" if all copied OK, E<nn> if error <nn> has occurred.

//! The length given is the number of bytes to be written. The data buffer has
//! already been unescaped, so will hold this number of bytes.

//! The data is in model-endian format, so no transformation is needed.
//-----------------------------------------------------------------------------
void GdbServer::rspWriteMemBin() {
  uint32_t addr;  // Where to write the memory
  int len;        // Number of bytes to write

  if (2 != sscanf(pkt->data, "X%x,%x:", &addr, &len)) {
    //spdlog::warn("GdbServer: Failed to recognize RSP write memory command: {:s}", std::string(pkt->data));
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  // Find the start of the data and "unescape" it. Bindat must be unsigned, or
  // all sorts of horrible sign extensions will happen when val is computed
  // below!
  uint8_t* bindat = (uint8_t*)(memchr(pkt->data, ':', pkt->getBufSize())) + 1;
  int off = (char*)bindat - pkt->data;
  int newLen = GdbServerUtils::rspUnescape((char*)bindat, pkt->getLen() - off);

  // Sanity check
  if (newLen != len) {
    int minLen = len < newLen ? len : newLen;

    cerr << "Warning: Write of " << len << " bytes requested, but " << newLen
      << " bytes supplied. " << minLen << " will be written" << endl;
    len = minLen;
  }
  else if (len == 0) {
    // spdlog::warn("Client requested 0-byte write to memory, ignoring.");
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;
  }

  // Write bytes to memory
  m_simCtrl->writeMem(bindat, addr, len);

  pkt->packStr("OK");
  rsp->putPkt(pkt);

}  // rspWriteMemBin ()

//-----------------------------------------------------------------------------
//! Handle a RSP remove breakpoint or matchpoint request
//-----------------------------------------------------------------------------
void GdbServer::rspRemoveMatchpoint() {
  MpType type;     // What sort of matchpoint
  uint32_t addr;   // Address specified
  uint32_t instr;  // Instruction value found
  int len;         // Matchpoint length (not used)

  // Break out the instruction
  if (3 != sscanf(pkt->data, "z%1d,%x,%1d", (int*)&type, &addr, &len)) {
    cerr << "Warning: RSP matchpoint deletion request not "
      << "recognized: ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  // Sanity check that the length is 2
  if (2 != len) {
    cerr << "Warning: RSP matchpoint deletion length " << len
      << "not valid: 2 assumed" << endl;
    len = 2;
  }

  // Sort out the type of matchpoint
  switch (type) {
  case BP_MEMORY:
    //        pkt->packStr ("");		// Not supported
    m_simCtrl->removeBreakpoint(addr);
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;

  case BP_HARDWARE:
    m_simCtrl->removeBreakpoint(addr);
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;

  case WP_WRITE:
    pkt->packStr("");  // Not supported
    rsp->putPkt(pkt);
    return;

  case WP_READ:
    pkt->packStr("");  // Not supported
    rsp->putPkt(pkt);
    return;

  case WP_ACCESS:
    pkt->packStr("");  // Not supported
    rsp->putPkt(pkt);
    return;

  default:
    cerr << "Warning: RSP matchpoint type " << type
      << " not recognized: ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }
}  // rspRemoveMatchpoint ()

//---------------------------------------------------------------------------*/
//! Handle a RSP insert breakpoint or matchpoint request

//! For now only memory breakpoints are implemented, which are implemented by
//! substituting a breakpoint at the specified address. The implementation must
//! cope with the possibility of duplicate packets.
//---------------------------------------------------------------------------*/
void GdbServer::rspInsertMatchpoint() {
  MpType type;    // What sort of matchpoint
  uint32_t addr;  // Address specified
  int len;        // Matchpoint length (not used)

  // Break out the instruction
  if (3 != sscanf(pkt->data, "Z%1d,%x,%1d", (int*)&type, &addr, &len)) {
    cerr << "Warning: RSP matchpoint insertion request not "
      << "recognized: ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }

  // Sanity check that the length is 2
  if (2 != len) {
    cerr << "Warning: RSP matchpoint insertion length " << len
      << "not valid: 2 assumed" << endl;
    len = 2;
  }

  // Sort out the type of matchpoint
  switch (type) {
  case BP_MEMORY:
    m_simCtrl->insertBreakpoint(addr);
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;

  case BP_HARDWARE:
    m_simCtrl->insertBreakpoint(addr);
    pkt->packStr("OK");
    rsp->putPkt(pkt);
    return;

  case WP_WRITE:
    pkt->packStr("");  // Not supported
    rsp->putPkt(pkt);
    return;

  case WP_READ:
    pkt->packStr("");  // Not supported
    rsp->putPkt(pkt);
    return;

  case WP_ACCESS:
    pkt->packStr("");  // Not supported
    rsp->putPkt(pkt);
    return;

  default:
    cerr << "Warning: RSP matchpoint type " << type
      << "not recognized: ignored" << endl;
    pkt->packStr("E01");
    rsp->putPkt(pkt);
    return;
  }
}  // rspInsertMatchpoint ()

