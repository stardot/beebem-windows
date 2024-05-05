/*
 * Copyright (c) 2018-2020, University of Southampton.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstdint>

 /**
 * @brief SimulationControlInterface Interface to control and interact with
 * simulation from an independent outside program, e.g. from a debug server.
 */
class SimulationControlInterface {
public:
  /* ------ Public methods ------ */

  virtual ~SimulationControlInterface() {};

  // Control and report
  /**
   * @brief Kill simulation.
   */
  virtual void kill() = 0;

  /**
   * @brief Reset simulation.
   */
  virtual void reset() = 0;

  /**
   * @brief stall execution.
   */
  virtual void stall() = 0;

  /**
   * @brief unstall execution.
   */
  virtual void unstall() = 0;

  /**
   * @brief check if execution is stalled.
   * @retval true if execution is stalled, false otherwise.
   */
  virtual bool isStalled() = 0;

  /**
   * @brief waitForCommand Wait for a new command. Target may call this after
   * single-stepping, hitting a breakpoint etc.
   */
   // virtual void waitForCommand() = 0;

   /**
    * @brief waitForTargetStalled Wait for the target to stall. Controller (e.g.
    * gdb server) may call this after issuing a single-step command, or to wait
    * for the target to hit a breakpoint, etc.
    */
    // virtual void waitForTargetStalled() = 0;

    /**
     * @brief step execute a single instruction, then stall
     */
  virtual void step() = 0;

  // Breakpoints
  /**
   * @brief insert a breakpoint, causing stall when PC=addr
   * @param addr address to stall at
   */
  virtual void insertBreakpoint(unsigned addr) = 0;

  /**
   * @brief Remove breakpoint at addr
   * @param addr address of breakpoint to remove.
   */
  virtual void removeBreakpoint(unsigned addr) = 0;

  // ------ Register access ------
  /**
   * @brief writeReg Read the contents of a general purpose register.
   * @param num register number.
   * @retval Content of register <num>.
   */
  virtual uint32_t readReg(std::size_t num) = 0;

  /**
   * @brief writeReg Set the contents of a general purpose register.
   * @param num register number
   * @param value value to write
   */
  virtual void writeReg(std::size_t num, uint32_t value) = 0;

  // ------ Memory access ------
  /**
   * @brief readMem read memory from target.
   * @param out output buffer
   * @param addr start address to read from
   * @param len number of bytes to read.
   */
  virtual bool readMem(uint8_t* out, unsigned addr, std::size_t len) = 0;

  /**
   * @brief writeMem write memory to target.
   * @param src source buffer
   * @param addr address to write to
   * @param len number of bytes to write
   * @retval bool true if success, 0 otherwise.
   */
  virtual bool writeMem(uint8_t* src, unsigned addr, std::size_t len) = 0;

  // ------ Target info ------
  /**
   * @brief pc_regnum get the register number of the program counter, i.e. which
   * register is the PC.
   * @retval register number of the program counter.
   */
  virtual uint32_t pcRegNum() = 0;

  /**
   * @brief n_regs get the number of general purpose registers.
   * @retval how many general purpose registers the target has (including PC,
   * SP, LR etc.)
   */
  virtual uint32_t nRegs() = 0;

  /**
   * @brief wordSize
   * @retval word size of target platform (in bytes)
   */
  virtual uint32_t wordSize() = 0;

  // ------ Control debugger ------

  /**
   * @brief stopServer Stop control server thread
   */
  virtual void stopServer() = 0;

  /**
   * @brief shouldStopServer Check if controller's server thread should stop.
   * @retval true if control thread should stop, false otherwise.
   */
  virtual bool shouldStopServer() = 0;

  /**
   * @brief isServerRunning Check if debug server is running.
   */
  virtual bool isServerRunning() = 0;

  /**
   * @brief setServerRunning set running status of debug server.
   * @param status Should be true if running, false otherwise.
   */
  virtual void setServerRunning(bool status) = 0;

  // ------ Target utilities ------
  /**
   * @brief htotl Convert value from host to target endianness
   * @param hostVal The value in host endianness
   * @return The value in target endianness
   */
  virtual uint32_t htotl(uint32_t hostVal) = 0;

  /**
   * @brief ttohl Convert value from target to host endianness
   * @param targetVal The value in target endianness
   * @return The value in host endianness
   */
  virtual uint32_t ttohl(uint32_t targetVal) = 0;
};
