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

};

