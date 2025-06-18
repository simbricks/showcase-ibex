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
#include <verilated_vcd_c.h>

#include "/lowrisc-ibex/ibex/obj_dir/Vibex_top.h"

#include "lib/utils/log.h"

extern "C"
{
#include <simbricks/mem/if.h>
#include <simbricks/mem/proto.h>
#include <simbricks/parser/parser.h>
}

#define IBEX_VERILATOR_DEBUG 1
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

void send_core_to_mem(struct SimbricksMemIf &memif, uint64_t cur_ts)
{
    volatile union SimbricksProtoMemH2M *msg = SimbricksMemIfH2MOutAlloc(&memif, cur_ts);
    if (msg == nullptr)
    {
#if IBEX_VERILATOR_DEBUG
        sim_log::LogWarn("send_core_to_mem msg nullptr\n");
#endif
        return;
    }
    // TODO: send either or depending on singlas
    // SimbricksMemIfH2MOutSend(&memif, msg, SIMBRICKS_PROTO_MEM_H2M_MSG_READ);
    // SimbricksMemIfH2MOutSend(&memif, msg, SIMBRICKS_PROTO_MEM_H2M_MSG_WRITE);
}

void poll_mem_to_core(struct SimbricksMemIf &memif, uint64_t cur_ts)
{

    volatile union SimbricksProtoMemM2H *msg = SimbricksMemIfM2HInPoll(&memif, cur_ts);
    if (msg == nullptr)
    {
#if IBEX_VERILATOR_DEBUG
        sim_log::LogWarn("poll_mem_to_core msg nullptr\n");
#endif
        return;
    }

    uint8_t type = SimbricksMemIfM2HInType(&memif, msg);
    switch (type)
    {
    case SIMBRICKS_PROTO_MEM_M2H_MSG_READCOMP:
        // TODO: implement me
        break;
    case SIMBRICKS_PROTO_MEM_M2H_MSG_WRITECOMP:
        // TODO: implement me
        break;
    case SIMBRICKS_PROTO_MSG_TYPE_SYNC:
        break;
    default:
        sim_log::LogError("poll_mem_to_core: unsupported type=%d", type);
    }

    SimbricksMemIfM2HInDone(&memif, msg);
}

void InitDut(Vibex_top &dut)
{
    // TODO
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

    if (SimbricksBaseIfSHMPoolCreate(
            &pool_, memAdapterParams->shm_path, SimbricksBaseIfSHMSize(&membase->params)) != 0)
    {
        perror("MemifInit: SimbricksBaseIfSHMPoolCreate failed");
        return false;
    }

    if (SimbricksBaseIfListen(membase, &pool_) != 0)
    {
        perror("MemifInit: SimbricksBaseIfListen failed");
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

    while (not exiting)
    {
        while (SimbricksMemIfH2MOutSync(&memif, main_time) != 0)
        {
            sim_log::LogError("warn: SimbricksMemIfH2MOutSync failed (t=%lu)\n", main_time);
        }

        do
        {
            poll_mem_to_core(memif, main_time);
        } while (not exiting and (memAdapterParams->sync and
                                  SimbricksMemIfM2HInTimestamp(&memif) <= main_time));

        /* falling edge */
        dut->clk_i = 0;
        dut->eval();
#ifdef CORUNDUM_VERILATOR_TRACE
        trace->dump(main_time);
#endif
        main_time += clock_period / 2;

        // evaluate on rising edge
        dut->clk_i = 1;
        send_core_to_mem(memif, main_time);
        dut->eval();

#if IBEX_VERILATOR_TRACE
        trace->dump(main_time);
#endif
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