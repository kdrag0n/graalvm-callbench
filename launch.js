// Load C benchmark code
let callbench = Polyglot.evalFile("llvm" ,"libcallbench.so");

// Allocate results struct
let results = callbench.allocResults();

// Run the benchmarks
console.log("Benchmarking clock_gettime...");
callbench.benchTime(results, 10000, 32, 3);
console.log("Benchmarking file I/O...");
callbench.benchFile(results, 100, 128, 3);

// Print results
console.log(`
clock_gettime:
    syscall: ${results.time_syscall_ns} ns
    libc: ${results.time_libc_ns} ns

read file:
    mmap: ${results.file_mmap_ns} ns
    read: ${results.file_read_ns} ns`)

// Free results
callbench.freeResults(results);
