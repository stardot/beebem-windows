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
#include <simulation_controller_interface.hpp>

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
  GdbServer(SimulationControlInterface* simCtrl, int rspPort);
  ~GdbServer();



};  // GdbServer ()

#endif  // GDB_SERVER_SC__H
