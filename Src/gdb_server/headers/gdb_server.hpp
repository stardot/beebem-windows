// ----------------------------------------------------------------------------

// SystemC GDB RSP server: definition

// Copyright (C) 2008  Embecosm Limited <info@embecosm.com>

// Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

// This file is part of the GDB interface to the cycle accurate model of the
// OpenRISC 1000 based system-on-chip, ORPSoC, built using Verilator.

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

// $Id: GdbServerSC.h 326 2009-03-07 16:47:31Z jeremy $


#ifndef GDB_SERVER_SC__H
#define GDB_SERVER_SC__H

#include <cstdint>
#include <rsp_connection.hpp>
#include <rsp_packet.hpp>
#include <i_simulation_control.hpp>

//! Module implementing a GDB RSP server.

//! A thread listens for RSP requests, which are converted to requests to read
//! and write registers, memory or control the CPU in the debug unit

class GdbServer {
public:
  // Constructor and destructor
  /**
   * @brief Constructor
   * @param simCtrl Pointer to simulation controller
   * @param rspPort gdb server listening port
   */
  GdbServer(ISimulationControl* simCtrl, int rspPort);
  ~GdbServer();

  // SystemC thread to listen for and service RSP requests
  void serverThread();


private:
  //! Definition of GDB target signals.

  //! Data taken from the GDB 6.8 source. Only those we use defined here.
  enum TargetSignal { TARGET_SIGNAL_NONE = 0, TARGET_SIGNAL_TRAP = 5 };

  //! Maximum size of a GDB RSP packet
  //  static const int  RSP_PKT_MAX  = NUM_REGS * 8 + 1;
  static const int RSP_PKT_MAX = 512;  // qSupported pkt can be >240 byte

  // OpenRISC exception addresses. Only the ones we need to know about
  static const uint32_t EXCEPT_NONE = 0x000;   //!< No exception
  static const uint32_t EXCEPT_RESET = 0x100;  //!< Reset

  //! Simulation control interface
  ISimulationControl* m_simCtrl;

  //! Our associated RSP interface (which we create)
  RspConnection* rsp;

  //! The packet pointer. There is only ever one packet in use at one time, so
  //! there is no need to repeatedly allocate and delete it.
  RspPacket* pkt;

  //! Is the target stopped
  bool targetStopped;

  // Main RSP request handler
  void rspClientRequest();

  // Handle the various RSP requests
  void rspReportException();
  void rspContinue();
  void rspContinue(uint32_t except);
  void rspContinue(uint32_t addr, uint32_t except);

  void rspReadAllRegs();
  void rspWriteAllRegs();
  void rspReadMem();
  void rspWriteMem();
  void rspReadReg();
  void rspWriteReg();
  void rspQuery();
  void rspCommand();
  void qSupported();
  void rspSet();
  void rspRestart();
  void rspStep();
  void rspStep(uint32_t except);
  void rspStep(uint32_t addr, uint32_t except);
  void rspVpkt();
  void rspWriteMemBin();
  void rspRemoveMatchpoint();
  void rspInsertMatchpoint();

  enum MpType {
    BP_MEMORY = 0,
    BP_HARDWARE = 1,
    WP_WRITE = 2,
    WP_READ = 3,
    WP_ACCESS = 4
  };




};  // GdbServer ()

#endif  // GDB_SERVER_SC__H
