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
#include <simulation_controller_interface.hpp>
#include <rsp_connection.hpp>
#include <rsp_packet.hpp>
#include <iomanip>
#include <iostream>
#include <thread>
