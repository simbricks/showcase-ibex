# Copyright 2025 Max Planck Institute for Software Systems, and
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

from simbricks.orchestration import system
from simbricks.orchestration import simulation
from simbricks.orchestration.helpers import simulation as sim_helpers
from simbricks.orchestration.helpers import instantiation as inst_helpers
from simbricks.utils import base as utils_base

"""
Import the orchestration bits we created as part of the Corundum integration.
Note that we must use the name of the package as present within the Docker Container
used by the Runner.
"""
from orchestration import ibex_orchestration as ibex


"""
This list is used and expected
"""
instantiations = []


"""
System Specification
"""
syst = system.System()

# create ibex core
core = ibex.IbexHost(syst)
core.name = "ibex-Core"

# create memory
mem = system.MemSimpleDevice(syst)
mem.name = "ibex-memory"
mem._load_elf = "/lowrisc-ibex/ibex/examples/sw/simple_system/hello_test/hello_test.elf"

# create terminal
terminal = system.MemTerminal(syst)
terminal.name = "terminal"

# create interconnect
ic = system.MemInterconnect(syst)
ic.name = "interconnect"
ic.connect_host(core._mem_if)
c = ic.connect_device(terminal._mem_if)
ic.add_route(c.host_if(), 0x20000, 0x1000)
c = ic.connect_device(mem._mem_if)
ic.add_route(c.host_if(), 0, mem._size)


"""
Simulator Choice
"""
sim = sim_helpers.simple_simulation(
    syst,
    compmap={
        ibex.IbexHost: ibex.IbexSim,
        system.MemSimpleDevice: simulation.BasicMem,
        system.MemTerminal: simulation.MemTerminal,
        system.MemInterconnect: simulation.BasicInterconnect,
    },
)
sim.name = 'ibex-sim'
sim.find_sim(core)._wait = True
sim.find_sim(ic).name = 'interconnect'

sim.enable_synchronization(500, utils_base.Time.Nanoseconds)

"""
Instantiation
"""
instance = inst_helpers.simple_instantiation(sim)

# Here we ensure that the runner does choose a proper docker image (the image defined in this repository)
# for executing the fragment we created.
fragment = instance.fragments[0]
fragment._fragment_executor_tag = "ibex_executor"

instantiations.append(instance)
