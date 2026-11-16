#!/usr/bin/env python3
"""Compare FM/r-index encoders with hash/segtree dictionaries and summarize resources."""

import argparse
from contextlib import contextmanager
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import traceback


REPO = Path(__file__).resolve().parents[1]
FIELDS = ["file", "index", "dictionary", "status", "exit_code", "input_bytes", "factor_count", "dictionary_entries",
          "index_serialized_bytes", "index_hash_estimated_bytes", "bwt_runs",
          "fm_serialized_bytes", "hash_allocated_bytes", "hash_peak_allocated_bytes",
          "fm_hash_estimated_bytes", "hash_buckets", "hash_load_factor", "rss_final_kib",
          "peak_rss_kib", "wall_seconds", "user_seconds", "system_seconds", "output_bytes",
          "reverse_seconds", "index_build_seconds", "fm_build_seconds", "factorize_seconds", "temporary_bytes_at_end",
          "report_directory", "error", "cleanup_error", "temporary_error", "report_error"]
PHASE_FIELDS = ["name", "seconds", "rss_begin_kib", "rss_end_kib", "process_peak_rss_kib"]
PROGRESS_FIELDS = ["processed_bytes", "factor_count", "dictionary_entries", "elapsed_seconds", "rss_kib",
                   "hash_allocated_bytes", "hash_peak_allocated_bytes", "hash_buckets",
                   "hash_load_factor", "hash_max_load_factor"]


@contextmanager
def atomic_text(path):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, prefix=f".{path.name}-",
                                         suffix=".tmp", delete=False, newline="", encoding="utf-8",
                                         errors="backslashreplace") as stream:
            temporary = Path(stream.name)
            yield stream
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            try:
                temporary.unlink()
            except FileNotFoundError:
                pass


def write_json(path, value):
    with atomic_text(path) as stream:
        json.dump(value, stream, ensure_ascii=True, indent=2)
        stream.write("\n")


def write_csv(path, rows, fields):
    with atomic_text(path) as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def read_time(path):
    result = {}
    labels = {"Maximum resident set size (kbytes)": "peak_rss_kib",
              "User time (seconds)": "user_seconds", "System time (seconds)": "system_seconds"}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        label, separator, value = line.strip().partition(": ")
        if separator and label in labels:
            result[labels[label]] = float(value) if "seconds" in labels[label] else int(value)
        elif label.startswith("Elapsed (wall clock) time") and separator:
            elapsed = 0.0
            for part in value.split(":"):
                elapsed = elapsed * 60 + float(part)
            result["wall_seconds"] = elapsed
    return result


def markdown_cell(value):
    return str(value).replace("\\", "\\\\").replace("|", "\\|").replace("\r", " ").replace("\n", " ")


def write_summary(batch, rows):
    write_csv(batch / "comparativo.csv", rows, FIELDS)
    columns = [("Arquivo", "file", 1), ("Índice", "index", 1), ("Dicionário", "dictionary", 1), ("Status", "status", 1),
               ("Entrada MiB", "input_bytes", 1048576), ("Fatores", "factor_count", 1),
               ("Entradas dicionário", "dictionary_entries", 1),
               ("Índice serializado MiB", "index_serialized_bytes", 1048576),
               ("Runs BWT", "bwt_runs", 1),
               ("Hash alocada MiB", "hash_allocated_bytes", 1048576),
               ("Pico hash MiB", "hash_peak_allocated_bytes", 1048576),
               ("Índice + hash estimativa MiB", "index_hash_estimated_bytes", 1048576),
               ("Pico RSS MiB", "peak_rss_kib", 1024),
               ("Construção s", "index_build_seconds", 1),
               ("Fatoração s", "factorize_seconds", 1), ("Tempo s", "wall_seconds", 1)]
    with atomic_text(batch / "comparativo.md") as stream:
        stream.write("# Medições do encoder atual\n\n")
        stream.write("Índice + hash é uma estimativa: índice serializado + alocações solicitadas pela hash. "
                     "Não representa RSS. Pico hash inclui alocações temporárias de rehash. "
                     "Para segtree, os campos de hash ficam vazios; o pico RSS mede o processo completo.\n\n")
        stream.write("| " + " | ".join(title for title, _, _ in columns) + " |\n")
        stream.write("| " + " | ".join("---" for _ in columns) + " |\n")
        for row in rows:
            cells = []
            for _, key, divisor in columns:
                value = row.get(key)
                if value is None or value == "":
                    cells.append("—")
                elif divisor != 1 or key == "wall_seconds":
                    cells.append(f"{float(value) / divisor:.3f}")
                else:
                    cells.append(markdown_cell(value))
            stream.write("| " + " | ".join(cells) + " |\n")


def git_value(*args):
    result = subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


def stop_process(process):
    # Stop the GNU Time wrapper and encoder together, including on interruption.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        pass
    # GNU Time may exit before the encoder. Always stop surviving members,
    # even when the wrapper has already been reaped.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def last_log_line(path):
    try:
        with path.open("rb") as stream:
            stream.seek(0, os.SEEK_END)
            stream.seek(max(0, stream.tell() - 8192))
            lines = stream.read().decode("utf-8", errors="replace").splitlines()
        return lines[-1] if lines else ""
    except OSError:
        return ""


class Runner:
    """Own the active process group, signal requests and durable batch diagnostics."""

    def __init__(self, batch):
        self.batch = batch
        self.process = None
        self.signum = None
        self.previous_handlers = {}
        self.console_enabled = True
        self.terminal_fd = None
        self.state = {"schema_version": 1, "pid": os.getpid(), "status": "running",
                      "completed_files": 0, "started_utc": datetime.now(timezone.utc).isoformat()}

    def __enter__(self):
        self.log = (self.batch / "runner.log").open("a", encoding="utf-8", errors="backslashreplace")
        # Reopen the TTY with an independent descriptor: changing flags on stdout
        # itself could change the shell's shared descriptor. Never wait on a paused TTY.
        if sys.stdout.isatty():
            try:
                self.terminal_fd = os.open(os.ttyname(sys.stdout.fileno()),
                                           os.O_WRONLY | os.O_NONBLOCK | os.O_NOCTTY)
            except OSError:
                self.console_enabled = False
        # Survive SSH/terminal hangups, including when launched with nohup.
        # Child encoders inherit SIG_IGN; explicit INT/TERM still stop the batch.
        self.previous_handlers[signal.SIGHUP] = signal.getsignal(signal.SIGHUP)
        signal.signal(signal.SIGHUP, signal.SIG_IGN)
        for signum in (signal.SIGINT, signal.SIGTERM):
            self.previous_handlers[signum] = signal.getsignal(signum)
            signal.signal(signum, self.request_stop)
        return self

    def request_stop(self, signum, frame):
        # Signal handlers only set a flag; do not interrupt a result-file replacement
        # or re-enter process termination / filesystem cleanup.
        if self.signum is None:
            self.signum = signum

    def __exit__(self, *args):
        try:
            if self.process is not None:
                stop_process(self.process)
        finally:
            for signum, handler in self.previous_handlers.items():
                signal.signal(signum, handler)
            if self.terminal_fd is not None:
                os.close(self.terminal_fd)
            self.log.close()

    def emit(self, message):
        self.log.write(f"{datetime.now(timezone.utc).isoformat()} {message}\n")
        self.log.flush()
        if not self.console_enabled:
            return
        if self.terminal_fd is not None:
            try:
                os.write(self.terminal_fd, (message + "\n").encode("utf-8", errors="backslashreplace"))
            except BlockingIOError:
                pass  # The complete message is already in runner.log.
            except OSError:
                self.console_enabled = False
            return
        try:
            print(message, flush=True)
        except (OSError, UnicodeError):
            self.console_enabled = False
            self.log.write("console unavailable; continuing with runner.log\n")
            self.log.flush()
            # A TextIOWrapper can retain the failed write and raise again at exit.
            try:
                with open(os.devnull, "w") as sink:
                    os.dup2(sink.fileno(), sys.stdout.fileno())
            except (OSError, ValueError):
                pass

    def update(self, **values):
        self.state.update(values)
        self.state["updated_utc"] = datetime.now(timezone.utc).isoformat()
        write_json(self.batch / "run_state.json", self.state)

    def fail(self, error):
        diagnostic = f"{type(error).__name__}: {error}"
        try:
            self.emit(f"Runner stopped: {diagnostic}\n{traceback.format_exc()}")
        except OSError:
            pass
        try:
            self.update(status="failed", stage="finished", error=diagnostic,
                        process_group_id=None)
        except OSError:
            pass


def wait_for_process(process, timeout, runner=None, stderr_path=None):
    # Retain the legacy log argument for external runners such as tudocomp.
    # Live progress no longer reads this file.
    started = time.monotonic()
    next_notice = started + 60
    deadline = started + timeout if timeout is not None else None
    while True:
        code = process.poll()
        if code is not None:
            return code, None
        if runner and runner.signum:
            stop_process(process)
            return process.returncode, "interrupted"
        now = time.monotonic()
        remaining = deadline - now if deadline is not None else None
        if remaining is not None and remaining <= 0:
            stop_process(process)
            return process.returncode, "timeout"
        if runner and now >= next_notice:
            label = runner.state.get("current_file") or "build"
            runner.emit(f"  {label}: em execução há {now - started:.0f}s")
            next_notice = now + 60
        try:
            return process.wait(timeout=min(0.2, remaining) if remaining is not None else 0.2), None
        except subprocess.TimeoutExpired:
            pass


def valid_number(value):
    return (not isinstance(value, bool) and
            ((isinstance(value, int) and value >= 0) or
             (isinstance(value, float) and math.isfinite(value) and value >= 0)))


def validate_metrics(metrics, input_size, output_size):
    if (not isinstance(metrics, dict) or type(metrics.get("schema_version")) is not int
            or metrics["schema_version"] not in (1, 2)):
        raise ValueError("unsupported metrics schema")
    for field in ("factor_count", "input_bytes", "output_bytes", "dictionary_entries"):
        if type(metrics.get(field)) is not int or metrics[field] < 0:
            raise ValueError(f"invalid {field}")
    index = metrics.get("index") if metrics["schema_version"] == 2 else "fm"
    if index not in ("fm", "rindex", "rlbwt"):
        raise ValueError("invalid index")
    if metrics["schema_version"] == 1:
        if type(metrics.get("fm_serialized_bytes")) is not int or metrics["fm_serialized_bytes"] < 0:
            raise ValueError("invalid fm_serialized_bytes")
    else:
        if type(metrics.get("index_serialized_bytes")) is not int or metrics["index_serialized_bytes"] <= 0:
            raise ValueError("invalid index_serialized_bytes")
        if index == "fm":
            if (type(metrics.get("fm_serialized_bytes")) is not int
                    or metrics["fm_serialized_bytes"] != metrics["index_serialized_bytes"]):
                raise ValueError("invalid fm_serialized_bytes")
            if metrics.get("bwt_runs") is not None:
                raise ValueError("FM report must not claim BWT runs")
        else:
            if metrics.get("fm_serialized_bytes") is not None or metrics.get("fm_hash_estimated_bytes") is not None:
                raise ValueError("non-FM report must not claim FM memory")
            if type(metrics.get("bwt_runs")) is not int or metrics["bwt_runs"] <= 0:
                raise ValueError("invalid bwt_runs")
    if metrics["factor_count"] * 9 != output_size:
        raise ValueError("factor count does not match output size")
    if metrics["output_bytes"] != output_size or metrics["input_bytes"] != input_size:
        raise ValueError("input/output sizes do not match metrics")
    for name, fields in (("phases", PHASE_FIELDS), ("progress_samples", PROGRESS_FIELDS)):
        records = metrics.get(name)
        if not isinstance(records, list):
            raise ValueError(f"{name} must be a list")
        for record in records:
            if not isinstance(record, dict) or set(record) != set(fields):
                raise ValueError(f"invalid {name} entry")
            for field, value in record.items():
                if field == "name":
                    if not isinstance(value, str):
                        raise ValueError("invalid phase name")
                elif value is None and field.startswith("hash_"):
                    continue
                elif not valid_number(value):
                    raise ValueError(f"invalid {name}.{field}")
    names = [phase["name"] for phase in metrics["phases"]]
    if sorted(names) != [f"build_{index}_index", "factorize", "reverse_input"]:
        raise ValueError("invalid phases")
    if metrics.get("dictionary") == "segtree":
        for record in [metrics, *metrics["progress_samples"]]:
            if any(value is not None for key, value in record.items()
                   if key.startswith("hash_") or key in ("index_hash_estimated_bytes", "fm_hash_estimated_bytes")):
                raise ValueError("segtree report must not claim hash memory")
    for key in FIELDS:
        if key in metrics and key not in {"file", "index", "dictionary", "status", "error", "report_directory"}:
            if metrics[key] is not None and not valid_number(metrics[key]):
                raise ValueError(f"invalid {key}")


def read_measurements(row, output, detail):
    try:
        row.update(read_time(detail / "time.txt"))
    except (OSError, ValueError) as error:
        if row["exit_code"] == 0 and row["status"] not in {"timeout", "interrupted"}:
            row.update(status="invalid_report", error=f"invalid GNU Time report: {error}")
            return None
        row.setdefault("error", f"GNU Time report unavailable: {error}")
    if row["exit_code"] != 0 or row["status"] in {"timeout", "interrupted"}:
        row.setdefault("error", last_log_line(detail / "stderr.log") or "encoder failed; see stderr.log")
        return None
    try:
        metrics = json.loads((detail / "metrics.json").read_text(encoding="utf-8"))
        validate_metrics(metrics, row["input_bytes"], output.stat().st_size)
        index = metrics.get("index") if metrics["schema_version"] == 2 else "fm"
        if index != row.get("index", "fm"):
            raise ValueError("reported index does not match requested index")
        if metrics.get("dictionary", "hash") != row.get("dictionary", "hash"):
            raise ValueError("reported dictionary does not match requested dictionary")
        if metrics["schema_version"] == 1:
            metrics.update(index="fm", index_serialized_bytes=metrics["fm_serialized_bytes"],
                           index_hash_estimated_bytes=metrics.get("fm_hash_estimated_bytes"), bwt_runs=None)
        if "peak_rss_kib" not in row:
            raise ValueError("GNU Time did not report peak RSS")
    except (OSError, ValueError, KeyError, TypeError) as error:
        row.update(status="invalid_report", error=str(error))
        return None
    reserved = {"file", "status", "exit_code", "input_bytes", "report_directory", "error",
                "cleanup_error", "temporary_error", "report_error", "temporary_bytes_at_end"}
    row.update({key: metrics[key] for key in FIELDS if key in metrics and key not in reserved})
    phase_times = {phase["name"]: phase["seconds"] for phase in metrics["phases"]}
    row.update(status="ok", reverse_seconds=phase_times["reverse_input"],
               index_build_seconds=phase_times[f"build_{index}_index"],
               fm_build_seconds=phase_times.get("build_fm_index"), factorize_seconds=phase_times["factorize"])
    return metrics


def cleanup_temporary(temporary, row):
    if temporary is None:
        return
    try:
        row["temporary_bytes_at_end"] = sum(path.stat().st_size
            for path in Path(temporary.name).rglob("*") if path.is_file())
    except OSError as error:
        row["temporary_error"] = f"{type(error).__name__}: {error}"
    finally:
        try:
            temporary.cleanup()
        except OSError as error:
            row["cleanup_error"] = f"{type(error).__name__}: {error}"


def measure_file(binary, source, output, detail, timeout, runner=None, index="fm", dictionary="hash"):
    row = {"file": source.name, "index": index, "dictionary": dictionary, "input_bytes": None, "exit_code": None,
           "report_directory": detail.name, "status": "failed"}
    temporary, process, metrics = None, None, None
    try:
        try:
            row["input_bytes"] = source.stat().st_size
            (detail / "time.txt").touch()
            temporary = tempfile.TemporaryDirectory(prefix="tmp-", dir=detail)
            index_args = [] if index == "fm" else ["--index", index]
            command = ["/usr/bin/time", "-v", "-o", str(detail / "time.txt"), str(binary),
                       "encode", str(source), str(output), dictionary, *index_args,
                       "--metrics", str(detail / "metrics.json")]
            # Keep metrics collection, but disable progress logging during benchmarks.
            environment = dict(os.environ, LC_ALL="C", LZ78_PROGRESS="0", TMPDIR=temporary.name)
            write_json(detail / "command.json", {"argv": command, "environment": {
                key: environment[key] for key in ("LC_ALL", "LZ78_PROGRESS", "TMPDIR")}})
            if runner and runner.signum:
                row.update(status="interrupted", error=signal.Signals(runner.signum).name)
            else:
                with (detail / "stdout.log").open("w") as stdout, (detail / "stderr.log").open("w") as stderr:
                    process = subprocess.Popen(command, stdout=stdout, stderr=stderr,
                                               env=environment, start_new_session=True)
                    if runner:
                        runner.process = process
                        runner.update(stage="encode", process_group_id=process.pid)
                    row["exit_code"], status = wait_for_process(process, timeout, runner)
                    if status:
                        row.update(status=status, error=signal.Signals(runner.signum).name
                                   if status == "interrupted" and runner else status)
                if runner:
                    runner.update(stage="read_metrics")
                metrics = read_measurements(row, output, detail)
        except KeyboardInterrupt:
            if process is not None:
                stop_process(process)
                row["exit_code"] = process.returncode
            if runner:
                runner.request_stop(signal.SIGINT, None)
            row.update(status="interrupted", error="SIGINT")
        except OSError as error:
            row.update(status="runner_error", error=f"{type(error).__name__}: {error}")
            if runner:
                runner.emit(f"{row['error']}\n{traceback.format_exc()}")
        finally:
            if process is not None:
                # Also terminate surviving members if GNU Time exited first.
                stop_process(process)
            if runner:
                runner.process = None
        # A completed attempt is durable before any auxiliary CSV / cleanup work.
        write_json(detail / "result.json", row)
        if metrics is not None:
            try:
                write_csv(detail / "phases.csv", metrics["phases"], PHASE_FIELDS)
                write_csv(detail / "progress.csv", metrics["progress_samples"], PROGRESS_FIELDS)
            except OSError as error:
                row["report_error"] = f"{type(error).__name__}: {error}"
        if runner:
            runner.update(stage="cleanup", process_group_id=None)
    finally:
        cleanup_temporary(temporary, row)
    for field in ("cleanup_error", "temporary_error", "report_error"):
        if row.get(field) and runner:
            runner.emit(f"{source.name}: {field}: {row[field]}")
    write_json(detail / "result.json", row)
    return row


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=REPO / "input")
    parser.add_argument("--output-dir", type=Path, default=REPO / "output")
    parser.add_argument("--reports-dir", type=Path, default=REPO / "reports")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--pattern", help="glob for files directly inside input-dir")
    selection.add_argument("--files", nargs="+", help="exact filenames inside input-dir, processed in the given order")
    parser.add_argument("--binary", type=Path, help="use an existing binary; skips automatic make")
    parser.add_argument("--timeout", type=float, help="maximum seconds per file")
    parser.add_argument("--indexes", nargs="+", choices=("fm", "rindex", "rlbwt"), default=["fm"],
                        help="indexes to measure sequentially for each file (default: fm)")
    parser.add_argument("--dictionaries", nargs="+", choices=("hash", "segtree"), default=["hash"],
                        help="dictionaries to measure sequentially for each file/index (default: hash)")
    args = parser.parse_args()
    args.indexes = ["rindex" if index == "rlbwt" else index for index in args.indexes]
    if len(set(args.indexes)) != len(args.indexes):
        parser.error("--indexes must not contain duplicates")
    if len(set(args.dictionaries)) != len(args.dictionaries):
        parser.error("--dictionaries must not contain duplicates")
    if args.timeout is not None and args.timeout <= 0:
        parser.error("--timeout must be positive")
    inputs = args.input_dir.resolve()
    outputs = args.output_dir.resolve()
    reports = args.reports_dir.resolve()
    if not inputs.is_dir():
        parser.error(f"input directory does not exist: {inputs}")
    if len({inputs, outputs, reports}) != 3:
        parser.error("input, output and reports directories must be different")
    if args.files is not None:
        if len(set(args.files)) != len(args.files):
            parser.error("--files must not contain duplicates")
        sources = []
        for name in args.files:
            if Path(name).name != name or name in (".", ".."):
                parser.error("--files accepts only filenames directly inside input-dir")
            source = inputs / name
            if not source.is_file():
                parser.error(f"input file does not exist: {source}")
            sources.append(source)
    else:
        sources = sorted(path for path in inputs.glob(args.pattern if args.pattern is not None else "*")
                         if path.is_file() and path.parent == inputs)
    if not sources:
        parser.error("no input files matched")
    attempts = [(source, index, dictionary) for source in sources
                for index in args.indexes for dictionary in args.dictionaries]
    if not Path("/usr/bin/time").is_file():
        parser.error("GNU Time is required at /usr/bin/time")
    binary = (args.binary or REPO / "main.x").resolve()
    reports.mkdir(parents=True, exist_ok=True)
    batch = Path(tempfile.mkdtemp(prefix=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-"), dir=reports))
    with Runner(batch) as runner:
        try:
            runner.update(stage="setup", files_total=len(attempts), current_file=None)
            runner.emit(f"Reports: {batch}")
            if args.binary is None:
                with (batch / "build.log").open("w") as build_log:
                    built = subprocess.Popen(["make", "main.x"], cwd=REPO, stdout=build_log,
                                             stderr=subprocess.STDOUT, start_new_session=True)
                    runner.process = built
                    try:
                        runner.update(stage="build", process_group_id=built.pid)
                        code, status = wait_for_process(built, None, runner)
                    finally:
                        stop_process(built)
                        runner.process = None
                if status == "interrupted":
                    runner.update(status="interrupted", stage="finished",
                                  signal=signal.Signals(runner.signum).name, process_group_id=None)
                    return 128 + runner.signum
                if code != 0:
                    raise RuntimeError(f"build failed; see {batch / 'build.log'}")
            if not binary.is_file() or not os.access(binary, os.X_OK):
                raise RuntimeError(f"encoder binary is not executable: {binary}")
            output_batch = outputs / batch.name
            output_batch.mkdir(parents=True, exist_ok=False)
            manifest = {"schema_version": 1, "started_utc": datetime.now(timezone.utc).isoformat(),
                        "binary": str(binary), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                        "git_commit": git_value("rev-parse", "HEAD"),
                        "git_status": git_value("status", "--short"),
                        "platform": list(os.uname()),
                        "input_directory": str(inputs), "output_directory": str(output_batch),
                        "files": [str(path) for path in sources], "indexes": args.indexes,
                        "dictionaries": args.dictionaries}
            write_json(batch / "run.json", manifest)
            rows = []
            for number, (source, index, dictionary) in enumerate(attempts, 1):
                if runner.signum:
                    break
                detail = batch / f"{number:04d}"
                detail.mkdir()
                runner.update(stage="prepare", current_file=source.name, current_index=index,
                              current_dictionary=dictionary, report_directory=detail.name,
                              process_group_id=None)
                runner.emit(f"[{number}/{len(attempts)}] {source.name} ({index}, {dictionary})")
                output_directory = output_batch
                if len(args.dictionaries) > 1:
                    output_directory /= dictionary
                if len(args.indexes) > 1:
                    output_directory /= index
                output_directory.mkdir(parents=True, exist_ok=True)
                row = measure_file(binary, source, output_directory / (source.name + ".bin"),
                                   detail, args.timeout, runner, index=index, dictionary=dictionary)
                rows.append(row)
                runner.update(stage="summary", completed_files=len(rows))
                write_summary(batch, rows)
                runner.emit(f"  {row['status']}" + (f": {row['error']}" if row.get("error") else ""))
                if row["status"] == "interrupted":
                    break
            if runner.signum:
                runner.update(status="interrupted", stage="finished",
                              signal=signal.Signals(runner.signum).name, process_group_id=None)
                runner.emit(f"Interrupted by {signal.Signals(runner.signum).name}")
                return 128 + runner.signum
            success = all(row["status"] == "ok" and not any(row.get(field) for field in
                          ("cleanup_error", "temporary_error", "report_error")) for row in rows)
            runner.update(status="completed" if success else "completed_with_errors", stage="finished",
                          process_group_id=None)
            runner.emit(f"Table: {batch / 'comparativo.md'}")
            return 0 if success else 1
        except Exception as error:
            runner.fail(error)
            return 2


def main():
    try:
        return run()
    except OSError as error:
        # Includes directory setup and Runner's log open/close: these can fail
        # before a logger exists or while unwinding an already recorded failure.
        try:
            print(f"Runner stopped: {type(error).__name__}: {error}", file=sys.stderr, flush=True)
        except (OSError, UnicodeError):
            pass
        return 2


if __name__ == "__main__":
    sys.exit(main())
