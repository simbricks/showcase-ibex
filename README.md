# Showcase for lowRISC Ibex integration into SimBricks

This repository contains all the necessary pieces for a simple integration of the [lowRISC Ibex](https://github.com/lowRISC/ibex) into SimBricks.

This includes:
- A memory adapter for the Ibex core using the simple memory protocol of SimBricks
- Integration of the Ibex core into the orchestration framework of SimBricks
- An application based on Ibex's `simple_system` example, that prints characters on a terminal
- A virtual prototype configuration script that connects the Ibex core with memory and a simple terminal simulator and runs the application

## Getting started

You can run the virtual prototype either via our [demo](https://www.simbricks.io/demo/) or locally on your machine.

### SimBricks demo

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/simbricks/showcase-ibex/?quickstart=1)

After creating a [SimBricks demo account](https://www.simbricks.io/demo/) you can open the devcontainer that is provided with this repository. In the devcontainer you just need to submit the virtual prototype configuration using the already installed `simbricks-cli` command:

```bash
simbricks-cli runs submit -f virtual_prototype.py
```

Instead of the devcontainer you can also install the necessary SimBricks Python packages locally (see [requirements.txt](requirements.txt)). For more information on that you can also look at the [README.md](https://github.com/simbricks/simbricks-examples/tree/main#clone-this-repository-and-set-up-a-python-virtual-environment) in the [simbricks-examples](https://github.com/simbricks/simbricks-examples) repository.

### Local

In order to run the virtual prototype configuration locally, the easiest approach is to use the docker image provided by the [Dockerfile](Dockerfile) in this repository. For this, build the docker image first, for example with the following command:

```bash
docker build -t showcase-ibex:latest .
```

This will

- build Ibex including our SimBricks adapter with Verilator
- build the application
- install the necessary SimBricks Python packages
- put the relevant files under `/lowrisc-ibex`
- have SimBricks with pre-built simulators under `/simbricks`

Now you can spin up a docker container and run the virtual prototype configuration:

```bash
docker run --rm -it --entrypoint /bin/bash showcase-ibex:latest
> cd /lowrisc-ibex
> simbricks-run --verbose virtual_prototype.py
```