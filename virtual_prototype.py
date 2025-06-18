# Copyright 2024 Max Planck Institute for Software Systems, and
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

# connect memory and core
memChannel = system.MemChannel(core._mem_if, mem._mem_if)


"""
Simulator Choice
"""
sim = sim_helpers.simple_simulation(
    syst,
    compmap={
        ibex.IbexHost: ibex.IbexSim,
        system.MemSimpleDevice: simulation., # TODO: simulator missing
    },
)


"""
Instantiation
"""
instance = inst_helpers.simple_instantiation(sim)
# Here we ensure that the runner does choose a proper docker image (the image defined in this repository)
# for executing the fragment we created.
fragment = instance.fragments[0]
fragment._fragment_executor_tag = "ibex_executor"

instantiations.append(instance)
