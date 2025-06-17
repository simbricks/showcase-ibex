
dir_ibex := ./ibex
ibex_out := $(dir_ibex)/build/ibex_out
verilator_dir_ibex := $(ibex_out)/obj_dir
verilog_interface_name := ibex_core
verilator_interface_name := V$(verilog_interface_name)
verilator_src_ibex := $(verilator_dir_ibex)/$(verilator_interface_name).cpp
verilator_bin_ibex := $(verilator_dir_ibex)/$(verilator_interface_name)

VERILATOR = verilator

$(ibex_out):
	cd ibex && fusesoc --cores-root . run --target=lint --setup --build-root ../$(ibex_out) lowrisc:ibex:ibex_top $(util/ibex_config.py small fusesoc_opts)

$(verilator_src_ibex): $(ibex_out)
	$(VERILATOR) --cc -O3 \
	    --Mdir $(verilator_dir_ibex) \
		--top-module $(verilog_interface_name) \
		--trace \
		-y $(ibex_out)/src/lowrisc_ibex_ibex_core_0.1/rtl
		+incdir+ $(ibex_out) \
		$(verilog_interface_name).sv

all: $(verilator_src_ibex)
.DEFAULT_GOAL := all

clean:
	rm -rf $(verilator_dir_ibex)

.PHONY: all clean