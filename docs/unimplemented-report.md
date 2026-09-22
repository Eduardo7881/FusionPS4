# UNIMPLEMENTED report

FusionPS4 never returns `0` to hide missing functionality. Every stub that
is not implemented reports the call through
`debug::UnimplementedRegistry`, which aggregates entries and prints a
report on shutdown.

## How it is triggered

The macro `FP4_UNIMPLEMENTED(category, symbol)` expands to a logging call
plus a registry update. The extended macro `FP4_UNIMPL(symbol, args,
note)` accepts a `"<library>::<function>"` key and free-form arguments.

Example call:

    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceAjmBatchJobDecode");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=hand-built AJM batches use a version-dependent layout";

The log message is emitted immediately with the requested category and
level; the registry accumulates.

## The registry

`UnimplementedRegistry::instance()` is a thread-safe map from key to
`Entry`:

    struct Entry {
        std::string   key;
        std::uint64_t count;
        std::string   lastArgs;
        std::string   lastNote;
    };

Entries are sorted by descending `count` when the report is formatted.

## The report

At shutdown, `Application::shutdown` calls:

    const auto unimpl = debug::UnimplementedRegistry::instance();
    if (unimpl.uniqueCount() > 0) {
        FP4_WARN(...) << "runtime exited with " << uniqueCount ...
        unimpl.dumpToStderr();
    }

The output looks like:

    FusionPS4 UNIMPLEMENTED report: 3 unique stubs, 14 total calls
    ---------------------------------------------------------------
      [10x] sceAjmBatchJobDecode
            note: hand-built AJM batches use a version-dependent layout
      [ 3x] GnmShader::parse(ORBIS)
            note: GCN ISA -> SPIR-V recompiler out of scope
      [ 1x] sceHttpAddRequestHeader
            args: name="Authorization"
            note: custom HTTP headers are not forwarded to libcurl

## Crash report

`debug::CrashHandler` also prints the current `UNIMPLEMENTED` report
before re-raising the signal. This makes it possible to see whether the
crash was caused by an unimplemented stub returning garbage. The report
is captured in the same process memory, so it is safe inside the signal
handler (the registry uses a mutex, which is not async-signal-safe in
general, but the handler runs with only the crashing thread alive and the
registry mutex is either free or held by a dead thread — in practice this
is acceptable for diagnostics).

## Reading the report

The `count` tells you how often the stub was hit. A stub with count 1 is
probably optional; a stub with count in the thousands is a hot path.

The `args` and `note` fields are populated by the code that emits the
report. Good practice is to include:

- the reason it is unimplemented (no recompiler, no hardware, ...),
- the specific argument that triggered the report (an unknown format id,
  an unsupported API, ...),
- the size or shape of the payload when it is safe to log.

## Tools

`fs4-unimpl-report <logfile>` reads a previously captured log and prints a
sorted summary of `UNIMPLEMENTED` lines. It is a simple grep-and-count; it
does not need the runtime to be running.
