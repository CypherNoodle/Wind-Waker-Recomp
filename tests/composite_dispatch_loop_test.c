#include "dispatch_loop.h"

#include <assert.h>
#include <string.h>

typedef struct Fixture {
    unsigned dispatches;
    unsigned yield_calls;
    unsigned yield_at;
    unsigned exception_at;
    unsigned miss_at;
    unsigned no_progress_at;
    u64 absolute_cycles;
    u64 delivery_deadline;
    u64 delivery_cycle;
    u32 delivery_pc;
} Fixture;

static Fixture fixture;

static int dispatch(CPUState* cpu, u32 address) {
    fixture.dispatches++;
    if (fixture.miss_at != 0u && fixture.dispatches == fixture.miss_at)
        return 0;
    cpu->pc = address + 4u;
    if (fixture.no_progress_at == 0u ||
        fixture.dispatches != fixture.no_progress_at)
        cpu->downcount -= 2;
    if (fixture.exception_at != 0u &&
        fixture.dispatches == fixture.exception_at)
        cpu->exception = 1u;
    return 1;
}

static bool edge_service(void* user, CPUState* cpu, u32 address) {
    Fixture* state = user;
    state->yield_calls++;
    if (state->delivery_deadline != 0u) {
        state->absolute_cycles += (u64)-cpu->downcount;
        cpu->downcount = 0;
        if (state->absolute_cycles >= state->delivery_deadline) {
            state->delivery_cycle = state->absolute_cycles;
            state->delivery_pc = address;
            return true;
        }
        const u64 distance = state->delivery_deadline - state->absolute_cycles;
        cpu->cycle_budget = distance < 8u ? (s64)distance : 8;
    }
    return state->yield_at != 0u && state->yield_calls == state->yield_at;
}

static void service_after_outer_return(CPUState* cpu) {
    if (fixture.delivery_cycle == 0u)
        (void)edge_service(&fixture, cpu, cpu->pc);
}

static CPUState fresh_cpu(void) {
    CPUState cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.cycle_budget = 8;
    return cpu;
}

int main(void) {
    CPUState cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, 0x80001000u, dispatch, NULL, NULL));
    assert(fixture.dispatches == 1u);

    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    fixture.yield_at = 2u;
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 2u);

    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 4u);
    assert(cpu.downcount == -8);

    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    fixture.exception_at = 3u;
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 3u);

    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    fixture.miss_at = 3u;
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 3u);

    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    fixture.no_progress_at = 2u;
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 2u);

    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    fixture.no_progress_at = 1u;
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 1u);

    // The shipping chassis does not call the exported entry point: it calls the
    // inline loop directly, so that the dispatch is a static function the
    // compiler can fold into the loop instead of a pointer it reloads at every
    // guest edge (cmake/composite/module_export.c). That is the path the product
    // runs, so it is checked here rather than assumed to match the wrapper: a
    // turn that ends because a block charged nothing, and one that ends on the
    // budget, have to finish the same way through both entry points.
    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    fixture.no_progress_at = 2u;
    assert(bluewake_chassis_dispatch_loop(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 2u);

    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    assert(bluewake_chassis_dispatch_loop(
        &cpu, 0x80001000u, dispatch, edge_service, &fixture));
    assert(fixture.dispatches == 4u);
    assert(cpu.downcount == -8);

    // A host service point on every edge makes delivery independent of
    // whether those edges return through the outer host loop.
    cpu = fresh_cpu();
    memset(&fixture, 0, sizeof(fixture));
    fixture.delivery_deadline = 6u;
    for (unsigned edge = 0u; edge < 3u; edge++) {
        assert(dispatch(&cpu, cpu.pc));
        service_after_outer_return(&cpu);
    }
    assert(fixture.delivery_cycle != 0u);
    const u64 outer_delivery_cycle = fixture.delivery_cycle;
    const u32 outer_delivery_pc = fixture.delivery_pc;

    cpu = fresh_cpu();
    cpu.pc = 0u;
    memset(&fixture, 0, sizeof(fixture));
    fixture.delivery_deadline = 6u;
    assert(bluewake_composite_dispatch_until_boundary(
        &cpu, cpu.pc, dispatch, edge_service, &fixture));
    service_after_outer_return(&cpu);
    assert(fixture.delivery_cycle == outer_delivery_cycle);
    assert(fixture.delivery_pc == outer_delivery_pc);
    return 0;
}
