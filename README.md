# What is branch miss and how it can hurt

* What exactly compiler optimizations are for?
* What happens if compiler fails to optimize the code properly?
* How to NOT defeat compiler optimizations?
* How to find the problematic code spots?

## Outline

* [Simple experiment](#simple-experiment)
* [Pipelining and out of the order execution](#pipelining-and-out-of-the-order-execution)
* [Finding branch misses with perf](#finding-branch-misses-with-perf)
* [Dealing with branch misses](#dealing-with-branch-misses)
* [Conclusion](#conclusion)

---

## Simple experiment

* Compute sum of integers 1, 2, ... 5000
* Compute partial sum: use only elements >= 2500

```python
import numpy as np

N = 5000
a = np.arange(1, N+1)
a.sum()
a.sum(where=a>=N/2)

np.random.shuffle(a)
a.sum()
a.sum(where=a>=N/2)
```

Questions:
* Which operation is faster?
* Does the order of elements matter? 

* `a.sum()`:     N additions
* `a.sum(where=a>=N/2)`: N/2 additions + N comparisons

Expectations: 
* `sum(where=a>=N/2)` is 1.5x slower
* The order of elements is irrelevant

```bash
python3 intro.py
```
<details>
<summary>Reality: (spoiler)</summary>

```
ordered all     : 1+2+...+5000     : 0.613 sec
ordered partial : >= 2500          : 1.184 sec
shuffled all    : 1839+1022+...+1299: 0.639 sec
shuffled partial: >= 2500          : 3.647 sec
```

Conditional sum is ~ 3 -- 6x slower with shuffled array. Why?
</details>

---

### Maybe it's a Python bug?

[complete source](rand_partsum.c)

```c
#define LOOPS 100000

long long rand_partsum(unsigned N, unsigned limit) {
	long long sum = 0;
	unsigned vec[N];
	for (unsigned i = 0; i < N; i++) {
		vec[i] = i + 1;
	}
	random_shuffle(vec, N);

	for (int k = 0; k < LOOPS; k++) {
		for (unsigned i = 0; i < N; i++) {
			if (vec[i] >= limit) {
				sum += vec[i];
			}
		}
	}
	return sum;
}
```

<details>
<summary>It's not very different with C</summary>

```bash
$ gcc-8 -O0 -static -o rand_partsum rand_partsum.c
$ time ./rand_partsum
real	0m3,457s
user	0m3,411s
sys	0m0,004s
```

```bash
$ gcc-8 -O0 -static -o partsum partsum.c 
$ time ./partsum
real	0m1,319s
user	0m1,302s
sys	0m0,000s
```
</details>

---

### More powerful tool: perf stat

Run the program and collect basic performance metrics

```bash
$ sudo sysctl -w kernel.perf_event_paranoid=0
$ perf stat -r 3 ./partsum
$ perf stat -r 3 ./rand_partsum
```

<details>
<pre>
 Performance counter stats for './partsum' (3 runs):

          1 272,01 msec task-clock                #    0,985 CPUs utilized            ( +-  0,08% )
                 5      context-switches          #    0,004 K/sec                    ( +- 12,50% )
                 0      cpu-migrations            #    0,000 K/sec                    ( +-100,00% )
                28      page-faults               #    0,022 K/sec                    ( +-  1,18% )
     3 553 269 510      cycles                    #    2,793 GHz                      ( +-  0,08% )
     5 753 454 730      instructions              #    1,62  insn per cycle           ( +-  0,00% )
     1 000 692 747      branches                  #  786,701 M/sec                    ( +-  0,00% )
           272 136      branch-misses             #    0,03% of all branches          ( +-  2,12% )

           1,29187 +- 0,00355 seconds time elapsed  ( +-  0,27% )
</pre>

<pre>
 Performance counter stats for './rand_partsum' (3 runs):

          3 203,43 msec task-clock                #    0,995 CPUs utilized            ( +-  0,16% )
                11      context-switches          #    0,003 K/sec                    ( +-  5,25% )
                 0      cpu-migrations            #    0,000 K/sec                    ( +-100,00% )
                28      page-faults               #    0,009 K/sec                  
     8 948 550 001      cycles                    #    2,793 GHz                      ( +-  0,16% )
     5 757 979 488      instructions              #    0,64  insn per cycle           ( +-  0,01% )
     1 001 672 449      branches                  #  312,688 M/sec                    ( +-  0,01% )
       179 510 980      branch-misses             #   <b>17,92%</b> of all branches          ( +-  0,09% )

           3,21938 +- 0,00910 seconds time elapsed  ( +-  0,28% )
</pre>
</details>

---


## Pipelining and out of the order execution

CPUs cheat: they don't execute the (machine code) program as is.

* Out of the order execution: run the instruction as soon as the necessary data is available
* Pipelining: split instruction into stages. Separate blocks handle each stage.
  Thus several instructions can make progress within a cycle.

These tricks are good if the CPU can guess which instructions will be executed soon.
Hence the branch prediction.

If the branch got mispredicted the pipeline must be reset (expensive!)


---

## Finding branch misses with perf

Often it's far from obvious where exactly branch misses occur.
Use a sampling profiler: `perf record`. Sample branch misses,
and visualize the result as a [flame graph](http://www.brendangregg.com/flamegraphs.html)

* What's a sampling profiler?
* How do I sample branch misses?
* What's a flame graph?

---

```bash
gcc-8 -O0 -g -static -o partsum_both partsum_both.c
perf record --event=branch-miss --call-graph=fp ./partsum_both -o randsum_both.data
perf script --header -i randsum_both.data > randsum_both.perfscript
stackcollapse-perf.pl randsum_both.perfscript > randsum_both.stacks
flamegraph.pl --title='Branch misses, no optimization' randsum_both.stacks > randsum_both.svg
```

* Sampling: interrupt program on an event, save program stack trace
* Flame graph: representation of collected stack traces

![branch misses flamegraph](partsum_both.svg)

---

### How sampling works (bird's-eye view)

* Hardware: Performance monitoring unit (PMU). Programmable event counters.
  Raise an interrupt on N events (CPU cycles, branch misses, cache misses, etc)
* Software: kernel: set counters, handle interrupts, etc
* Software: perf: further event processing (which process was on CPU,
  walk stack trace, record the sample)

---

### Obtaining stack traces

#### Option 1: frame pointers

```
perf record --event=branch-miss --call-graph=fp ./partsum_both -o randsum_both.data
```

* Debug info (compile with `-g`)
* Frame pointers (`-fno-omit-frame-pointer`)

Pros:

+ Low run time overhead
+ Does not depend on advanced PMU features

Cons:

- Compilers don't preserve frame pointers by default
- All libraries used by app should be compiled with frame pointers


#### Option 2: DWARF debug info

```
perf record --event=branch-miss --call-graph=dwarf ./partsum_both -o randsum_both.data
```

* Debug info (compile with `-g`)

Pros:

+ Distro software can be profiled
+ Does not depend on advanced PMU features

Cons:

- Extreme run time overhead
- Truncated stack traces

 
#### Option 3: advanced PMU features

```
perf record --event=branch-miss --call-graph=lbr ./partsum_both -o randsum_both.data
```

Pros:

+ Negligible run time overhead
+ Does not need debug info

Cons:

- Available only in some CPUs (Intel Haswell)
- Only userspace stack can be recorded


#### Kernel part of stack trace

* `System.map` should be readable
* `/proc/kallsyms` should be readable (`kernel.kptr_restrict=0`)


## Dealing with branch misses

* [Sort the input](#sort-the-input)
* [Rewrite the code without branches](#rewrite-the-code-without-branches)
* [Enable optimizations](#enable-optimizations)

### Sort the input

Branch miss happens only once (approximately after N/2 elements)

### Swap the loops

The same branch is taken 100000 in a row

```c
for (unsigned i = 0; i < size; i++) {
	for (int k = 0; k < LOOPS; k++) {
		if (vec[i] >= limit) {
			sum += vec[i];
		}
	}
}
```

### Rewrite the code without branches

```c
unsigned step2(unsigned x, unsigned y) {
    return (1U ^ ((x - y) >> 31);
}

sum += vec[i]*step2(vec[i], limit);
```

This makes more calculations (including multiplication), but without
branch misses the CPU runs 3x instructions per a second

---

### Enable optimizations

```bash
$ gcc-8 -O2 -static -o rand_partsum_opt rand_partsum.c
$ perf stat -r3 ./rand_partsum_opt
```

<details>

<pre>

 Performance counter stats for './rand_partsum_opt' (3 runs):

            370,13 msec task-clock                #    0,971 CPUs utilized            ( +-  1,24% )
                 5      context-switches          #    0,014 K/sec                    ( +- 16,54% )
                 0      cpu-migrations            #    0,001 K/sec                    ( +-100,00% )
                28      page-faults               #    0,077 K/sec                    ( +-  1,18% )
     1 032 315 732      cycles                    #    2,789 GHz                      ( +-  1,31% )
     4 002 103 254      instructions              #    3,88  insn per cycle           ( +-  0,00% )
       500 476 292      branches                  # 1352,179 M/sec                    ( +-  0,00% )
           109 957      branch-misses             #    0,02% of all branches          ( +-  0,14% )

           0,38122 +- 0,00428 seconds time elapsed  ( +-  1,12% )
</pre>
<pre>
 Performance counter stats for './partsum_opt' (3 runs):

            373,75 msec task-clock                #    0,973 CPUs utilized            ( +-  0,57% )
                 4      context-switches          #    0,012 K/sec                    ( +- 15,38% )
                 0      cpu-migrations            #    0,000 K/sec                  
                28      page-faults               #    0,076 K/sec                    ( +-  1,18% )
     1 038 527 951      cycles                    #    2,779 GHz                      ( +-  0,21% )
     4 001 546 307      instructions              #    3,85  insn per cycle           ( +-  0,00% )
       500 316 183      branches                  # 1338,640 M/sec                    ( +-  0,00% )
           109 015      branch-misses             #    0,02% of all branches          ( +-  0,23% )

           0,38415 +- 0,00238 seconds time elapsed  ( +-  0,62% )

</pre>
 </details>

Almost 10x faster than non-optimized variant. Why?
`if (vec[i] >= limit)` -> `cmovge` (conditional move instruction)

```asm
<+104>:	mov    0xc(%rsp),%r15d      # %r15: N
<+109>:	mov    $0x186a0,%r9d        # 0x186a0 == 100000
<+115>:	xor    %r13d,%r13d          # %r13: sum = 0
<+118>:	lea    0x4(%rbp,%r15,4),%r8 # %rbp: &vec[0], %r8: %rbp + 4*%r15 + 4 = &vec[0] + N
<+123>:	nopl   0x0(%rax,%rax,1)

<+128>:	mov    %rbp,%rdx
<+131>:	test   %r12d,%r12d
<+134>:	je     0x400cd8 <rand_partsum+168>
<+136>:	nopl   0x0(%rax,%rax,1)

<+144>:	movslq (%rdx),%rcx   # %rdx: &vec[i], %rcx <- vec[i] 
<+147>:	mov    %rcx,%rsi     # %rsi <- vec[i]
<+150>:	add    %r13,%rcx     # %r13: sum, %rcx = sum + vec[i]
<+153>:	cmp    %ebx,%esi     # %ebx: limit (= N/2); limit > vec[i] ?
<+155>:	cmovge %rcx,%r13     # if yes write %rcx to %r13
<+159>:	add    $0x4,%rdx     # %rdx points to next element
<+163>:	cmp    %r8,%rdx      # %r8: &vec[0] + N
<+166>:	jne    0x400cc0 <rand_partsum+144>

<+168>:	sub    $0x1,%r9d
<+172>:	jne    0x400cb0 <rand_partsum+128>
```

---

## Conclusion

* More is less: just because the program exectues more instructions does NOT mean it's slower
  - Measure instructions per cycle, branch misses
* `perf stat` is your friend
* Don't benchmark `Debug` builds, ever
* Better not use `Debug` builds at all
* Premature optimization is root of all evil

---

### P.S. Processor performance is not measured in GHz

The following table lists the run time of non-optimized (compiled with `-O0`)
`partsum` and `rand_partsum` measured with `perf stat` on different CPUs/boards:

|                  | Intel Core i7-7700HQ  | AMD Ryzen 5 2600  | Baikal-T1 (MIPS P5600) |  U740 (RISC-V)   |  U540 (RISC-V)   |
| :--------------- | :-------------------: | :---------------: | :--------------------: | :--------------: | :--------------: |
| instructions     |  5 756 616 461        |  5 755 856 610    |  12 522 218 790        |  10 554 022 119  |  10 533 849 293  |
| seconds user     |  0.96  /   2.40       |  1.09  / 1.66     |  6.20  / 11.35         |  9.46  / 10.82   |  16.10 / 17.32   |
| insns per cycle  |  1.63  /   0.66       |  1.40  / 0.90     |  1.69  /  0.85         |  0.93  /  0.81   |   0.65 /  0.61   |
| branch miss, %   |  0.03% /  17,32%      |  0.02% / 8.20%    |  0.03% / 23.93%        |    -- N/A --     |     -- N/A --    |
| clock rate       |  2.8 GHz              |  3.4 GHz          |  1.2 GHz               |  1 GHz           |  1 GHz           |


(branch miss counts are not available for RISC-V boards due to kernel issues)

|                  | Loongson 3A5000  |
| :--------------- | :--------------: |
| instructions     |  10 262 426 188  |
| seconds user     |  1.85  /  4.26   |
| insns per cycle  |  2.22  /  0.96   |
| branch miss, %   |  0.10% / 23.51%  |
| clock rate       |  2.5 GHz         |


- Baikal-T1 is indeed superscalar and out-of-order.
  Thus `partsum` is almost 2x faster than `rand_partsum` on this SoC.
- U740 is superscalar, but in-order. So superscalar execution makes `partsum` only 15% faster
- U540 is in-order with a classical pipeline. No superscalar execution here.
- The simple the core the less the difference between `partsum` and `rand_partsum` run time is

- U740, U540 have the same clock rate, however the `partsum` benchmark runs 1.7x faster on U740
- Baikal-T1 runs `partsum` 1.5x faster than U740, however the clock rate of Baikal-T1 is only 1.2x faster
- The clock rate of Core i7-7700HQ is 2.3 faster than that of Baikal-T1,
  however Core i7 runs `partsum` almost 10x faster.
