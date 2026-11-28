# Domscript benchmarks

Run the runtime trace-tier benchmark with:

```sh
GOROOT=/usr/lib/go GOCACHE=/tmp/domscript-gocache \
  go test -tags keystone ./benchmark -run '^$' -bench BenchmarkLoop -benchmem
```

`BenchmarkLoop` compares the baseline interpreter with hot thresholds of 1,
10, and 50 executions, plus a no-guard trace configuration. Compilation is
outside the timed region; each case executes the same compiled chunk.
