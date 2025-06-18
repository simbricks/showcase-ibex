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

dir_ibex := ./ibex
verilator_dir_ibex := $(dir_ibex)/obj_dir
verilog_interface_name := ibex_top
verilator_interface_name := V$(verilog_interface_name)
verilator_src_ibex := $(verilator_dir_ibex)/$(verilator_interface_name).cc
verilator_bin_ibex := $(verilator_dir_ibex)/$(verilator_interface_name)
adapter_main := adapter/ibex_simbricks
ibex_simbricks_adapter_src := $(adapter_main).cpp
ibex_simbricks_adapter_bin := $(adapter_main)

simbricks_base ?= /simbricks
lib_dir := $(simbricks_base)/lib
simbricks_lib_dir := $(lib_dir)/simbricks
lib_mem := $(simbricks_lib_dir)/mem/libmem.a
lib_base := $(simbricks_lib_dir)/base/libbase.a
lib_parser := $(simbricks_lib_dir)/parser/libparser.a

VERILATOR = verilator
VFLAGS = 


$(verilator_src_ibex):
	$(VERILATOR) $(VFLAGS) --cc -O3 \
		-CFLAGS "-I$(abspath $(lib_dir)) -iquote $(simbricks_base) -O3 -g -Wall -Wno-maybe-uninitialized" \
		--Mdir $(verilator_dir_ibex) \
		--top-module $(verilog_interface_name) \
		--trace \
		-y $(dir_ibex)/rtl/ \
		-y $(dir_ibex)/vendor/lowrisc_ip/ip/prim/rtl/ \
		-y $(dir_ibex)/vendor/lowrisc_ip/ip/prim_generic/rtl/ \
		-y $(dir_ibex)/vendor/lowrisc_ip/dv/sv/dv_utils/ \
		-y $(dir_ibex)/dv/uvm/core_ibex/common/prim/ \
		$(dir_ibex)/rtl/ibex_pkg.sv \
		$(dir_ibex)/vendor/lowrisc_ip/ip/prim/rtl/*_pkg.sv \
		$(dir_ibex)/dv/uvm/core_ibex/common/prim/prim_pkg.sv \
		$(dir_ibex)/rtl/$(verilog_interface_name).sv \
		--exe $(abspath $(ibex_simbricks_adapter_src)) $(abspath $(lib_mem) $(lib_base) $(lib_parser))

#$(dir_ibex)/rtl/ibex_pkg.sv $(dir_ibex)/vendor/lowrisc_ip/ip/prim/rtl/prim_ram_1p_pkg.sv $(dir_ibex)/vendor/lowrisc_ip/ip/prim/rtl/prim_secded_pkg.sv $(dir_ibex)/vendor/lowrisc_ip/ip/prim/rtl/prim_util_pkg.sv $(dir_ibex)/vendor/lowrisc_ip/ip/prim/rtl/prim_mubi_pkg.sv $(dir_ibex)/vendor/lowrisc_ip/ip/prim/rtl/prim_cipher_pkg.sv $(dir_ibex)/rtl/$(verilog_interface_name).sv \

$(verilator_bin_ibex): $(verilator_src_ibex) $(ibex_simbricks_adapter_src)
	$(MAKE) -C $(verilator_dir_ibex) -f $(verilator_interface_name).mk

$(ibex_simbricks_adapter_bin): $(verilator_bin_ibex)
	cp $< $@


all: $(ibex_simbricks_adapter_bin)
.DEFAULT_GOAL := all

clean: 
	rm -rf $(ibex_simbricks_adapter_bin) $(verilator_dir_ibex) $(OBJS)

.PHONY: all driver adapter clean