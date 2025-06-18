# Copyright 2021 Max Planck Institute for Software Systems, and
# National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

from __future__ import annotations

import typing_extensions as tpe
from simbricks.utils import base as utils_base
from simbricks.orchestration import system as sys
from simbricks.orchestration.simulation import base as sim_base
from simbricks.orchestration.instantiation import base as inst_base


# System Configuration Integration


class IbexHost(sys.Component):
    def __init__(self, s: sys.System) -> None:
        super().__init__(s)
        self._mem_if: sys.MemHostInterface = sys.MemHostInterface(self)

    def toJSON(self) -> dict:
        json_obj = super().toJSON()
        json_obj["mem_if"] = self._mem_if.id()
        return json_obj

    @classmethod
    def fromJSON(cls, system: sys.System, json_obj: dict) -> tpe.Self:
        instance = super().fromJSON(system, json_obj)
        mem_if_id = int(utils_base.get_json_attr_top(json_obj, "mem_if"))
        instance._mem_if = system.get_inf(mem_if_id)
        return instance


# Simulation Configuration Integration


class IbexSim(sim_base.Simulator):

    def __init__(self, simulation: sim_base.Simulation):
        super().__init__(
            simulation=simulation,
            executable="/lowrisc-ibex/adapter/ibex_simbricks",
        )
        self.name = f"IbexSim-{self._id}"
        self.clock_freq = 250  # MHz

    def resreq_mem(self) -> int:
        # this is a guess
        return 512

    def run_cmd(self, inst: inst_base.Instantiation) -> str:
        ibex_comps = self.filter_components_by_type(ty=IbexHost)
        ibex_comp = ibex_comps[0]

        channels = self.get_channels()
        mem_channels = sim_base.Simulator.filter_channels_by_sys_type(
            channels, sys.MemChannel
        )
        mem_latency, mem_sync_period, mem_run_sync = (
            sim_base.Simulator.get_unique_latency_period_sync(mem_channels)
        )
        socket = inst.get_socket(interface=ibex_comp._mem_if)
        mem_params_url = self.get_parameters_url(
            inst,
            socket,
            sync=mem_run_sync,
            latency=mem_latency,
            sync_period=mem_sync_period,
        )

        cmd = (
            f"{self._executable} {mem_params_url} {self._start_tick} {self.clock_freq}"
        )
        return cmd

    def toJSON(self) -> dict:
        json_obj = super().toJSON()
        json_obj["clock_freq"] = self.clock_freq
        return json_obj

    @classmethod
    def fromJSON(cls, simulation: sim_base.Simulation, json_obj: dict) -> tpe.Self:
        instance = super().fromJSON(simulation, json_obj)
        instance.clock_freq = utils_base.get_json_attr_top(json_obj, "clock_freq")
        return instance
