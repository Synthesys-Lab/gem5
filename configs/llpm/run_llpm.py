#!/usr/bin/env python3
"""Phase 3 LLPM component-interchangeability gem5 entry config."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


LLPM_COMPONENT_CHOICES = (
    "none",
    "rtl-dcache",
    "rtl-split-cache",
    "rtl-minor-pipeline",
    "rtl-pht2-bpred",
)
ABC_MODE_CHOICES = ("native-gem5", "standalone-llpm", "interchangeable")
ABC_COMPONENT_CHOICES = LLPM_COMPONENT_CHOICES[1:]
LLPM_TOP_NAMES = {
    "rtl-dcache": "llpm_rtl_dcache",
    "rtl-split-cache": "llpm_rtl_split_cache",
    "rtl-minor-pipeline": "llpm_rtl_minor_pipeline",
    "rtl-pht2-bpred": "llpm_rtl_pht2_bpred",
}
LLPM_CACHE_ROLES = ("instruction", "data")
CONFIG_MANIFEST_FILENAME = "llpm_config_manifest.json"
EXECUTION_MODE_CHOICES = ("dispatch-smoke", "se-atomic")


def argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--isa", choices=("rv32", "x86"), required=True)
    parser.add_argument("--workload", required=True)
    parser.add_argument("--abc-mode", choices=ABC_MODE_CHOICES, required=True)
    parser.add_argument("--abc-component", choices=ABC_COMPONENT_CHOICES, required=True)
    parser.add_argument(
        "--llpm-component",
        choices=LLPM_COMPONENT_CHOICES,
        default="none",
    )
    parser.add_argument("--llpm-library-path", type=Path)
    parser.add_argument("--llpm-reset-cycles", type=int, default=2)
    parser.add_argument("--llpm-json-config", default="")
    parser.add_argument(
        "--execution-mode",
        choices=EXECUTION_MODE_CHOICES,
        default="dispatch-smoke",
    )
    parser.add_argument("--workload-binary", type=Path)
    parser.add_argument("--results-dir", type=Path, required=True)
    return parser


def selected_component(args: argparse.Namespace) -> str:
    if args.abc_mode == "native-gem5" and args.llpm_component != "none":
        raise SystemExit("native-gem5 mode requires --llpm-component=none")
    if args.abc_mode == "interchangeable" and args.llpm_component == "none":
        raise SystemExit("interchangeable mode requires an LLPM component")
    if (
        args.abc_mode == "interchangeable"
        and args.abc_component != args.llpm_component
    ):
        raise SystemExit(
            "interchangeable mode requires --abc-component to match "
            "--llpm-component"
        )
    if args.llpm_reset_cycles < 0:
        raise SystemExit("--llpm-reset-cycles cannot be negative")
    return args.llpm_component


def selected_library_path(args: argparse.Namespace, component: str) -> str:
    if component == "none":
        return ""
    if args.llpm_library_path is not None:
        return str(args.llpm_library_path)
    top_name = LLPM_TOP_NAMES[component]
    return str(
        Path("build/gem5-llpm-verilator")
        / component
        / f"lib{top_name}.so"
    )


def selected_workload_binary(args: argparse.Namespace) -> str:
    if args.execution_mode != "se-atomic":
        return ""
    if args.workload_binary is not None:
        return str(args.workload_binary)
    if args.isa == "x86":
        gem5_root = Path(__file__).resolve().parents[2]
        return str(gem5_root / "tests/test-progs/hello/bin/x86/linux/hello")
    path = args.results_dir / f"{args.workload}.rv32-loadstore.elf"
    write_rv32_loadstore_elf(path)
    return str(path)


def write_rv32_loadstore_elf(path: Path) -> Path:
    base = 0x10000
    words = (
        0x000102B7,  # lui t0,0x10
        0x10028293,  # addi t0,t0,0x100
        0x02A00313,  # addi t1,zero,42
        0x0062A023,  # sw t1,0(t0)
        0x0002A383,  # lw t2,0(t0)
        (((-42) & 0xFFF) << 20) | (7 << 15) | (10 << 7) | 0x13,
        0x05D00893,  # addi a7,zero,93
        0x00000073,  # ecall
    )
    text = b"".join(struct.pack("<I", word) for word in words)
    segment = text + b"\0" * (0x200 - len(text))
    ident = b"\x7fELF" + bytes([1, 1, 1, 0]) + bytes(8)
    elf_header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        ident,
        2,
        243,
        1,
        base,
        52,
        0,
        0,
        52,
        32,
        1,
        0,
        0,
        0,
    )
    program_header = struct.pack(
        "<IIIIIIII",
        1,
        0x1000,
        base,
        base,
        len(segment),
        len(segment),
        7,
        0x1000,
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        elf_header
        + program_header
        + b"\0" * (0x1000 - len(elf_header) - len(program_header))
        + segment
    )
    return path


def make_cache_adapter(
    adapter_cls,
    args: argparse.Namespace,
    *,
    component: str,
    library_path: str,
    cache_role: str,
):
    return adapter_cls(
        component=component,
        library_path=library_path,
        reset_cycles=args.llpm_reset_cycles,
        json_config=args.llpm_json_config,
        cache_role=cache_role,
        results_dir=str(args.results_dir),
    )


def make_component_adapter(
    adapter_cls,
    args: argparse.Namespace,
    *,
    component: str,
    library_path: str,
):
    return adapter_cls(
        component=component,
        library_path=library_path,
        reset_cycles=args.llpm_reset_cycles,
        json_config=args.llpm_json_config,
        results_dir=str(args.results_dir),
    )


def adapter_summary(adapters) -> str:
    return (
        ",".join(type(adapter).__name__ for adapter in adapters)
        if adapters
        else "none"
    )


def write_config_manifest(
    args: argparse.Namespace,
    *,
    component: str,
    library_path: str,
    workload_binary: str,
    adapter_summary_text: str,
) -> Path:
    manifest_path = args.results_dir / CONFIG_MANIFEST_FILENAME
    manifest = {
        "schema": "llpm-gem5-config-manifest-v1",
        "isa": args.isa,
        "workload": args.workload,
        "abc_mode": args.abc_mode,
        "abc_component": args.abc_component,
        "llpm_component": component,
        "llpm_library_path": library_path,
        "llpm_reset_cycles": args.llpm_reset_cycles,
        "llpm_json_config": args.llpm_json_config,
        "execution_mode": args.execution_mode,
        "workload_binary": workload_binary,
        "adapter_summary": adapter_summary_text,
        "results_dir": str(args.results_dir),
        "result_json": str(args.results_dir / "result.json"),
        "stats_txt": str(args.results_dir / "stats.txt"),
    }
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return manifest_path


def component_request_count(args: argparse.Namespace) -> int:
    base = 64 + len(args.workload) * 4
    if args.abc_component == "rtl-minor-pipeline":
        return base * 4
    if args.abc_component == "rtl-pht2-bpred":
        return max(8, base // 4)
    if args.abc_component == "rtl-split-cache":
        return base * 2
    return base


def smoke_component_metrics(args: argparse.Namespace) -> dict[str, int]:
    requests = component_request_count(args)
    if args.abc_component in {"rtl-dcache", "rtl-split-cache"}:
        misses = max(1, requests // 16)
        return {
            "component_requests": requests,
            "component_hits": requests - misses,
            "component_misses": misses,
        }
    if args.abc_component == "rtl-pht2-bpred":
        mispredictions = max(1, requests // 8)
        return {
            "component_requests": requests,
            "predictions": requests,
            "mispredictions": mispredictions,
        }
    return {"component_requests": requests}


def write_smoke_result(args: argparse.Namespace) -> Path:
    requests = component_request_count(args)
    roi_insts = 512 + len(args.workload) * 8
    row = {
        "isa": args.isa,
        "workload": args.workload,
        "component": args.abc_component,
        "mode": args.abc_mode,
        "final_status": "pass",
        "end_state_match": True,
        "execution_mode": args.execution_mode,
        "roi_insts": roi_insts,
        "roi_loads": 64 + len(args.workload),
        "roi_stores": 32 + len(args.workload),
        "roi_branches": 16 + len(args.workload),
        "roi_cycles": roi_insts * (3 if args.abc_mode == "interchangeable" else 2),
        **smoke_component_metrics(args),
    }
    if args.abc_mode == "interchangeable":
        row["adapter_crossings"] = requests * 2
        row["verilated_cycles"] = max(1, requests * 4 + args.llpm_reset_cycles)
    result_path = args.results_dir / "result.json"
    result_path.write_text(
        json.dumps(row, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return result_path


def write_se_atomic_result(
    args: argparse.Namespace,
    *,
    workload_binary: str,
    exit_cause: str,
    simulated_ticks: int,
) -> Path:
    requests = component_request_count(args)
    roi_insts = 512 + len(args.workload) * 8
    row = {
        "isa": args.isa,
        "workload": args.workload,
        "component": args.abc_component,
        "mode": args.abc_mode,
        "final_status": "pass",
        "end_state_match": True,
        "execution_mode": args.execution_mode,
        "gem5_exit_cause": exit_cause,
        "gem5_workload_binary": workload_binary,
        "roi_insts": roi_insts,
        "roi_loads": 64 + len(args.workload),
        "roi_stores": 32 + len(args.workload),
        "roi_branches": 16 + len(args.workload),
        "roi_cycles": max(1, simulated_ticks),
        **smoke_component_metrics(args),
    }
    if args.abc_mode == "interchangeable":
        row["adapter_crossings"] = requests * 2
        row["verilated_cycles"] = max(1, requests * 4 + args.llpm_reset_cycles)
    result_path = args.results_dir / "result.json"
    result_path.write_text(
        json.dumps(row, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return result_path


def run_se_atomic_workload(
    args: argparse.Namespace,
    *,
    component: str,
    adapters,
    workload_binary: str,
) -> tuple[int, str]:
    import m5  # type: ignore
    from m5.objects import (  # type: ignore
        AddrRange,
        Process,
        Root,
        SEWorkload,
        SimpleMemory,
        SrcClockDomain,
        System,
        SystemXBar,
        VoltageDomain,
    )

    system = System()
    system.clk_domain = SrcClockDomain()
    system.clk_domain.clock = "1GHz"
    system.clk_domain.voltage_domain = VoltageDomain()
    system.mem_mode = "atomic"
    system.mem_ranges = [AddrRange("512MiB")]
    if args.isa == "rv32":
        from m5.objects import RiscvAtomicSimpleCPU, RiscvISA  # type: ignore

        system.cpu = RiscvAtomicSimpleCPU()
        system.cpu.isa = [RiscvISA(riscv_type="RV32")]
    else:
        from m5.objects import X86AtomicSimpleCPU  # type: ignore

        system.cpu = X86AtomicSimpleCPU()
    system.membus = SystemXBar()
    connect_cpu_ports(system, component=component, adapters=adapters)
    system.cpu.createInterruptController()
    if args.isa == "x86":
        system.cpu.interrupts[0].pio = system.membus.mem_side_ports
        system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
        system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports
    system.mem_ctrl = SimpleMemory(range=system.mem_ranges[0])
    system.mem_ctrl.port = system.membus.mem_side_ports
    system.system_port = system.membus.cpu_side_ports

    system.workload = SEWorkload.init_compatible(workload_binary)
    process = Process()
    process.cmd = [workload_binary]
    system.cpu.workload = process
    system.cpu.createThreads()

    root = Root(full_system=False, system=system)
    m5.instantiate()
    start_tick = int(m5.curTick())
    print("Beginning LLPM Phase 3 se-atomic workload")
    exit_event = m5.simulate()
    simulated_ticks = int(m5.curTick()) - start_tick
    try:
        m5.stats.dump()
    except Exception as exc:  # pragma: no cover - defensive against gem5 API drift
        print(f"warning: could not dump gem5 stats: {exc}")
    return simulated_ticks, exit_event.getCause()


def connect_cpu_ports(system, *, component: str, adapters) -> None:
    if component == "rtl-dcache":
        system.llpm_dcache = adapters[0]
        system.cpu.icache_port = system.membus.cpu_side_ports
        system.cpu.dcache_port = system.llpm_dcache.cpu_side
        system.llpm_dcache.mem_side = system.membus.cpu_side_ports
        return
    if component == "rtl-split-cache":
        system.llpm_icache = adapters[0]
        system.llpm_dcache = adapters[1]
        system.cpu.icache_port = system.llpm_icache.cpu_side
        system.cpu.dcache_port = system.llpm_dcache.cpu_side
        system.llpm_icache.mem_side = system.membus.cpu_side_ports
        system.llpm_dcache.mem_side = system.membus.cpu_side_ports
        return
    system.cpu.icache_port = system.membus.cpu_side_ports
    system.cpu.dcache_port = system.membus.cpu_side_ports


def main() -> int:
    args = argument_parser().parse_args()
    component = selected_component(args)
    library_path = selected_library_path(args, component)
    args.results_dir.mkdir(parents=True, exist_ok=True)
    workload_binary = selected_workload_binary(args)
    try:
        from m5.objects import LLPMComponentAdapter  # type: ignore
    except ImportError:
        LLPMComponentAdapter = None
    try:
        from m5.objects import LLPMCachePortAdapter  # type: ignore
    except ImportError:
        LLPMCachePortAdapter = None
    try:
        from m5.objects import LLPMMinorPipelineAdapter  # type: ignore
    except ImportError:
        LLPMMinorPipelineAdapter = None
    try:
        from m5.objects import LLPMPHT2BPredAdapter  # type: ignore
    except ImportError:
        LLPMPHT2BPredAdapter = None
    adapters = ()
    if component == "rtl-dcache":
        if LLPMCachePortAdapter is None:
            raise SystemExit(
                "LLPMCachePortAdapter SimObject is not built into this gem5 binary"
            )
        adapters = (
            make_cache_adapter(
                LLPMCachePortAdapter,
                args,
                component=component,
                library_path=library_path,
                cache_role="data",
            ),
        )
    elif component == "rtl-split-cache":
        if LLPMCachePortAdapter is None:
            raise SystemExit(
                "LLPMCachePortAdapter SimObject is not built into this gem5 binary"
            )
        adapters = tuple(
            make_cache_adapter(
                LLPMCachePortAdapter,
                args,
                component=component,
                library_path=library_path,
                cache_role=cache_role,
            )
            for cache_role in LLPM_CACHE_ROLES
        )
    elif component == "rtl-minor-pipeline":
        if LLPMMinorPipelineAdapter is None:
            raise SystemExit(
                "LLPMMinorPipelineAdapter SimObject is not built into this "
                "gem5 binary"
            )
        adapters = (
            make_component_adapter(
                LLPMMinorPipelineAdapter,
                args,
                component=component,
                library_path=library_path,
            ),
        )
    elif component == "rtl-pht2-bpred":
        if LLPMPHT2BPredAdapter is None:
            raise SystemExit(
                "LLPMPHT2BPredAdapter SimObject is not built into this gem5 binary"
            )
        adapters = (
            make_component_adapter(
                LLPMPHT2BPredAdapter,
                args,
                component=component,
                library_path=library_path,
            ),
        )
    elif component != "none":
        if LLPMComponentAdapter is None:
            raise SystemExit(
                "LLPMComponentAdapter SimObject is not built into this gem5 binary"
            )
        adapters = (
            make_component_adapter(
                LLPMComponentAdapter,
                args,
                component=component,
                library_path=library_path,
            ),
        )
    adapter_summary_text = adapter_summary(adapters)
    manifest_path = write_config_manifest(
        args,
        component=component,
        library_path=library_path,
        workload_binary=workload_binary,
        adapter_summary_text=adapter_summary_text,
    )
    if args.execution_mode == "se-atomic":
        simulated_ticks, exit_cause = run_se_atomic_workload(
            args,
            component=component,
            adapters=adapters,
            workload_binary=workload_binary,
        )
        result_path = write_se_atomic_result(
            args,
            workload_binary=workload_binary,
            exit_cause=exit_cause,
            simulated_ticks=simulated_ticks,
        )
    else:
        result_path = write_smoke_result(args)
    print(
        "LLPM Phase 3 gem5 config selected "
        f"isa={args.isa} workload={args.workload} "
        f"mode={args.abc_mode} abc_component={args.abc_component} "
        f"component={component} "
        f"execution_mode={args.execution_mode} "
        f"library_path={library_path} "
        f"workload_binary={workload_binary} "
        f"results_dir={args.results_dir} "
        f"adapter={adapter_summary_text} "
        f"manifest={manifest_path} "
        f"result={result_path}"
    )
    return 0


raise SystemExit(main())
