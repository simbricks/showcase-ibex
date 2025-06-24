/*
 * Copyright 2024 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <cassert>
#include <verilated_vcd_c.h>

#include <Vibex_top.h>

#include "lib/utils/log.h"

extern "C"
{
#include <simbricks/mem/if.h>
#include <simbricks/mem/proto.h>
#include <simbricks/parser/parser.h>
}

#define IBEX_VERILATOR_DEBUG 0
#define IBEX_VERILATOR_TRACE 0
#define IBEX_VERILATOR_TRACE_LEVEL 40

/* **************************************************************************
 * signal handling
 * ************************************************************************** */

static uint64_t main_time = 0;
static volatile bool exiting = 0;
static void sigint_handler([[maybe_unused]] int _dummy)
{
    exiting = true;
}
static void sigusr1_handler([[maybe_unused]] int _dummy)
{
    sim_log::LogError("main_time = %lu\n", main_time);
}

#define INSTR_REQ_ID 0
#define DATA_REQ_ID 1
#define DATA_WRITE_ID 2

static bool pending_instr_req = false;
static bool pending_data = false;
static bool pending_data_write = false;

struct delayed {
    bool instr_rvalid_i;
    uint32_t instr_rdata_i;
    bool data_rvalid_i;
    uint32_t data_rdata_i;
};

void send_core_to_mem(struct SimbricksMemIf &memif, uint64_t cur_ts, Vibex_top &dut, delayed &delay)
{
    volatile union SimbricksProtoMemH2M *msg;

    delay.instr_rvalid_i = 0;
    delay.data_rvalid_i = 0;

    // handel instruction read
    if (dut.instr_req_o and not pending_instr_req)
    {
        msg = SimbricksMemIfH2MOutAlloc(&memif, cur_ts);
        if (msg == nullptr)
        {
    #if IBEX_VERILATOR_DEBUG
            sim_log::LogWarn("send_core_to_mem msg nullptr\n");
    #endif
            return;
        }

        volatile struct SimbricksProtoMemH2MRead &read = msg->read;
        read.addr = dut.instr_addr_o;
        read.req_id = INSTR_REQ_ID;
        read.len = 4;
        pending_instr_req = true;
#if IBEX_VERILATOR_DEBUG
        sim_log::LogInfo("[%lu] send_core_to_mem instruction read addr=%lx len=%u nullptr\n", main_time, dut.instr_addr_o, 4);
        sim_log::FlushLog();
#endif
        SimbricksMemIfH2MOutSend(&memif, msg, SIMBRICKS_PROTO_MEM_H2M_MSG_READ);
    }

    // handel data read
    if (dut.data_req_o and not dut.data_we_o and not pending_data)
    {
        msg = SimbricksMemIfH2MOutAlloc(&memif, cur_ts);
        if (msg == nullptr)
        {
    #if IBEX_VERILATOR_DEBUG
            sim_log::LogWarn("send_core_to_mem msg nullptr\n");
    #endif
            return;
        }

        volatile struct SimbricksProtoMemH2MRead &read = msg->read;
        read.addr = dut.data_addr_o;
        read.req_id = DATA_REQ_ID;
        read.len = 4; // TODO: bytes enabled
        pending_data = true;

#if IBEX_VERILATOR_DEBUG
        sim_log::LogInfo("[%lu] send_core_to_mem data read addr=%lx len=%u nullptr\n", main_time, dut.data_addr_o, 4);
        sim_log::FlushLog();
#endif
        SimbricksMemIfH2MOutSend(&memif, msg, SIMBRICKS_PROTO_MEM_H2M_MSG_READ);
    }
    // handel data write
    else if (dut.data_req_o and dut.data_we_o and not pending_data)
    {
        if (dut.data_addr_o == 0x20008 && dut.data_wdata_o == 1) {
            exiting = true;
            return;
        }
        msg = SimbricksMemIfH2MOutAlloc(&memif, cur_ts);
        if (msg == nullptr)
        {
    #if IBEX_VERILATOR_DEBUG
            sim_log::LogWarn("send_core_to_mem msg nullptr\n");
    #endif
            return;
        }

        volatile struct SimbricksProtoMemH2MWrite &write = msg->write;
        write.addr = dut.data_addr_o;
        write.req_id = DATA_WRITE_ID;
        if (dut.data_be_o == 1)
            write.len = 1;
        else if (dut.data_be_o == 2)
            write.len = 2;
        else
            write.len = 4;
        memcpy(const_cast<uint8_t *>(write.data), &dut.data_wdata_o, write.len);

#if IBEX_VERILATOR_DEBUG
        sim_log::LogInfo("[%lu] send_core_to_mem data write addr=%lx len=%u nullptr\n", main_time, dut.data_addr_o, 4);
        sim_log::FlushLog();
#endif
        SimbricksMemIfH2MOutSend(&memif, msg, SIMBRICKS_PROTO_MEM_H2M_MSG_WRITE_POSTED);
        pending_data = true;
        pending_data_write = true;
    } else if (pending_data_write) {
        // complete pending write from prior cycle
        pending_data_write = false;
        pending_data = false;
        delay.data_rvalid_i = 1;
    }
}

void poll_mem_to_core(struct SimbricksMemIf &memif, uint64_t cur_ts, Vibex_top &dut, delayed &delay)
{

    volatile union SimbricksProtoMemM2H *msg = SimbricksMemIfM2HInPoll(&memif, cur_ts);
    if (msg == nullptr)
    {
#if IBEX_VERILATOR_DEBUG
//      sim_log::LogWarn("poll_mem_to_core msg nullptr\n");
#endif
        return;
    }

    uint8_t type = SimbricksMemIfM2HInType(&memif, msg);
    switch (type)
    {
    case SIMBRICKS_PROTO_MEM_M2H_MSG_READCOMP:
    {
        volatile struct SimbricksProtoMemM2HReadcomp &readcomp = msg->readcomp;
        if (readcomp.req_id == INSTR_REQ_ID)
        {
            assert(!delay.instr_rvalid_i);
            delay.instr_rvalid_i = 1;
            memcpy(&delay.instr_rdata_i, const_cast<uint8_t *>(readcomp.data), 4);
            pending_instr_req = false;
#if IBEX_VERILATOR_DEBUG
            sim_log::LogInfo("[%lu] poll_mem_to_core inst mem read complete (%x)\n", main_time, dut.instr_rdata_i);
            sim_log::FlushLog();
#endif
        }
        else
        {
            assert(readcomp.req_id == DATA_REQ_ID);
            assert(!delay.data_rvalid_i);
            delay.data_rvalid_i = 1;
            memcpy(&delay.data_rdata_i, const_cast<uint8_t *>(readcomp.data), 4);
            pending_data = false;
#if IBEX_VERILATOR_DEBUG
            sim_log::LogInfo("[%lu] poll_mem_to_core data mem read complete (%x)\n", main_time, dut.data_rdata_i);
            sim_log::FlushLog();
#endif
        }
        break;
    }
    case SIMBRICKS_PROTO_MEM_M2H_MSG_WRITECOMP:
        // NOTE: currently we only support posted writes
        break;
    case SIMBRICKS_PROTO_MSG_TYPE_SYNC:
        break;
    default:
        sim_log::LogError("poll_mem_to_core: unsupported type=%d", type);
    }

    SimbricksMemIfM2HInDone(&memif, msg);
}

void init_dut(Vibex_top &dut, delayed &delay)
{
    // Clock and Reset
    dut.clk_i = 0;
    dut.rst_ni = 0;

    dut.scan_rst_ni = 1;

    dut.hart_id_i = 0;
    dut.boot_addr_i = 0x00100080;

    // Instruction memory interface
    // output logic instr_req_o,
    dut.instr_gnt_i = 0;
    dut.instr_rvalid_i = 0;
    delay.instr_rvalid_i = 0;
    // input logic instr_gnt_i,
    // input logic instr_rvalid_i,
    // output logic [31:0] instr_addr_o,
    dut.instr_rdata_i = 0;
    dut.instr_rdata_intg_i = 0;
    dut.instr_err_i = 0;
    // input logic [31:0] instr_rdata_i,
    // input logic [6:0] instr_rdata_intg_i,
    // input logic instr_err_i,

    // Data memory interface
    // output logic data_req_o,
    dut.data_gnt_i = 0;
    dut.data_rvalid_i = 0;
    delay.data_rvalid_i = 0;
    // input logic data_gnt_i,
    // input logic data_rvalid_i,
    // output logic data_we_o,
    // output logic [3:0] data_be_o,
    // output logic [31:0] data_addr_o,
    // output logic [31:0] data_wdata_o,
    // output logic [6:0] data_wdata_intg_o,
    dut.data_rdata_i = 0;
    dut.data_rdata_intg_i = 0;
    dut.data_err_i = 0;
    // input logic [31:0] data_rdata_i,
    // input logic [6:0] data_rdata_intg_i,
    // input logic data_err_i,

    // CPU Control Signals
    dut.fetch_enable_i = 5;
    // input ibex_mubi_t fetch_enable_i,
    // output logic alert_minor_o,
    // output logic alert_major_internal_o,
    // output logic alert_major_bus_o,
    // output logic core_sleep_o,

    dut.rst_ni = 0;
    dut.eval();

    /* raising edge */
    dut.clk_i = 1;
    dut.eval();

    dut.clk_i = 0;
    dut.rst_ni = 1;
    dut.instr_gnt_i = 1;
    dut.data_gnt_i = 1;
}

void CheckAlerts(Vibex_top &dut)
{
  if (dut.alert_minor_o) {
    perror("Core error: Minor Alert");
    abort();
  }
  if (dut.alert_major_internal_o) {
    perror("Core error: Major Internal Alert");
    abort();
  }
  if (dut.alert_major_bus_o) {
    perror("Core error: Major Bus Alert");
    abort();
  }
}

bool MemifInit(struct SimbricksMemIf &memif, struct SimbricksAdapterParams *memAdapterParams)
{
    struct SimbricksBaseIfParams memParams;
    SimbricksMemIfDefaultParams(&memParams);
    if (memAdapterParams->sync_interval_set)
    {
        memParams.sync_interval = memAdapterParams->sync_interval * 1000ULL;
    }
    if (memAdapterParams->link_latency_set)
    {
        memParams.link_latency = memAdapterParams->link_latency * 1000ULL;
    }
    memParams.sock_path = memAdapterParams->socket_path;
    memParams.sync_mode = memAdapterParams->sync ? kSimbricksBaseIfSyncRequired : kSimbricksBaseIfSyncDisabled;
    memParams.blocking_conn = true;

    struct SimbricksBaseIf *membase = &memif.base;
    struct SimbricksBaseIfSHMPool pool_;
    memset(&pool_, 0, sizeof(pool_));

    struct SimBricksBaseIfEstablishData ests[1];
    struct SimbricksProtoMemHostIntro m_intro;
    struct SimbricksProtoMemHostIntro h_intro;
    unsigned n_bifs = 0;

    memset(&m_intro, 0, sizeof(m_intro));
    ests[n_bifs].base_if = membase;
    ests[n_bifs].tx_intro = &m_intro;
    ests[n_bifs].tx_intro_len = sizeof(m_intro);
    ests[n_bifs].rx_intro = &h_intro;
    ests[n_bifs].rx_intro_len = sizeof(h_intro);
    n_bifs++;

    if (SimbricksBaseIfInit(membase, &memParams))
    {
        perror("Init: SimbricksBaseIfInit failed");
        return false;
    }

    if (SimbricksBaseIfConnect(membase) != 0)
    {
        perror("MemifInit: SimbricksBaseIfConnect failed");
        return false;
    }

    if (SimBricksBaseIfEstablish(ests, 1))
    {
        fprintf(stderr, "SimBricksBaseIfEstablish failed\n");
        return false;
    }

#if IBEX_VERILATOR_DEBUG
    sim_log::LogInfo("done establishing mem connection\n");
#endif
    return true;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigint_handler);
    signal(SIGUSR1, sigusr1_handler);

#if IBEX_VERILATOR_DEBUG
    sim_log::LogRegistry().SetFlush(true);
#endif

    auto dut = std::make_unique<Vibex_top>();
#if IBEX_VERILATOR_TRACE
    auto trace = std::make_unique<VerilatedVcdC>();
    Verilated::traceEverOn(true);
    dut->trace(trace.get(), IBEX_VERILATOR_TRACE_LEVEL);
    trace->open("ibex-verilator-debug.vcd");
#endif

    // argument parsing and initialization
    uint64_t clock_period = 4 * 1000ULL; // 4ns -> 250MHz
    if (argc < 2 or argc > 4)
    {
        fprintf(stderr, "Usage: ibex_simbricks MEM-PARAMS [START-TICK] [CLOCK-FREQ-MHZ]\n");
        return EXIT_FAILURE;
    }
    if (argc == 3)
    {
        main_time = strtoull(argv[2], NULL, 0);
    }
    if (argc == 4)
    {
        clock_period = 1000000ULL / strtoull(argv[3], NULL, 0);
    }

    struct SimbricksAdapterParams *memAdapterParams = nullptr;
    memAdapterParams = SimbricksParametersParse(argv[1]);
    if (not memAdapterParams)
    {
        fprintf(stderr, "Failed to parse mem parameters\n");
        return EXIT_FAILURE;
    }

    // initialize SimBricks memory protocol
    struct SimbricksMemIf memif;
    if (not MemifInit(memif, memAdapterParams))
    {
#if IBEX_VERILATOR_DEBUG
        sim_log::LogError("could not init mem interface\n");
#endif
        SimbricksParametersFree(memAdapterParams);
        return EXIT_FAILURE;
    }

    // initialize and reset the dut
    delayed delay;
    init_dut(*dut, delay);

    while (not exiting)
    {
        while (SimbricksMemIfH2MOutSync(&memif, main_time) != 0)
        {
            sim_log::LogError("warn: SimbricksMemIfH2MOutSync failed (t=%lu)\n", main_time);
        }

        send_core_to_mem(memif, main_time, *dut, delay);
        do
        {
            poll_mem_to_core(memif, main_time, *dut, delay);
        } while (not exiting and (memAdapterParams->sync and
                                  SimbricksMemIfM2HInTimestamp(&memif) <= main_time));

        /* evaluate on raising edge */
        dut->clk_i = 1;
        dut->eval();
#if IBEX_VERILATOR_TRACE
        trace->dump(main_time);
#endif
        CheckAlerts(*dut);
        main_time += clock_period / 2;

        dut->instr_rvalid_i = delay.instr_rvalid_i;
        dut->instr_rdata_i = delay.instr_rdata_i;
        dut->instr_gnt_i = !pending_instr_req;
        dut->data_rvalid_i = delay.data_rvalid_i;
        dut->data_rdata_i = delay.data_rdata_i;
        dut->data_gnt_i = !pending_data;

        // falling edge
        dut->clk_i = 0;
        dut->eval();

#if IBEX_VERILATOR_TRACE
        trace->dump(main_time);
#endif
        CheckAlerts(*dut);
        main_time += clock_period / 2;
    }

#if IBEX_VERILATOR_TRACE
    trace->dump(main_time + 1);
    trace->close();
#endif

    dut->final();

    SimbricksParametersFree(memAdapterParams);

    return EXIT_SUCCESS;
}