Dr. CU
======================================
Dr. CU is a VLSI detailed routing tool developed by the research team supervised by Prof. Evangeline F. Y. Young in The Chinese University of Hong Kong (CUHK). Different from global routing, detailed routing takes care of many detailed design rules and is performed on a significantly larger routing grid graph. In advanced technology nodes, it becomes the most complicated and time-consuming stage in the VLSI physical design flow. To tackle the challenges, we design and implement several efficient and effective data structures and algorithms under a holistic framework:
* A set of two-level sparse data structures
* An optimal correct-by-construction path search
* An efficient bulk synchronous parallel scheme
* ...

More details are in the following papers:

* Gengjie Chen, Chak-Wa Pui, Haocheng Li, Jingsong Chen, Bentian Jiang and Evangeline F. Y. Young,
["Detailed Routing by Sparse Grid Graph and Minimum-Area-Captured Path Search"](https://doi.org/10.1145/3287624.3287678),
IEEE/ACM Asia and South Pacific Design Automation Conference (ASPDAC), Tokyo, Japan, Jan 21-24, 2019.
* Gengjie Chen, Chak-Wa Pui, Haocheng Li and Evangeline F. Y. Young,
"Dr. CU: Detailed Routing by Sparse Grid Graph and Minimum-Area-Captured Path Search",
submitted to IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems (TCAD).

(This version of code supports [ISPD'18 benchmarks](http://www.ispd.cc/contests/18/#benchmarks) and is consistent with the TCAD submission.)

## 1. How to Build

**Step 1:** Download the source code. For example,
~~~
$ git clone https://github.com/cuhk-route/dr-cu
~~~

**Step 2:** Go to the project root and build by
~~~
$ cd dr-cu
$ ./scripts/build.py -o release
~~~

Note that this will generate two folders under the root, `build` and `run` (`build` contains intermediate files for build/compilation, while `run` contains binaries and auxiliary files).
More details are in [`scripts/build.py`](scripts/build.py).

### 1.1. Dependencies

* [GCC](https://gcc.gnu.org/) (version >= 5.5.0) or other working c++ compliers
* [CMake](https://cmake.org/) (version >= 2.8)
* [Boost](https://www.boost.org/) (version >= 1.58)
* [Python](https://www.python.org/) (version 3, optional, for utility scripts)
* [Innovus®](https://www.cadence.com/content/cadence-www/global/en_US/home/tools/digital-design-and-signoff/soc-implementation-and-floorplanning/innovus-implementation-system.html) (version 17.1, optional, for design rule checking and evaluation)
* [Rsyn](https://github.com/RsynTeam/rsyn-x) (a trimmed version is used, already added under folder `rsyn`)

## 2. How to Run

### 2.1. Toy Test

#### Run Binary Directly

Go to the `run` directory and run the binary `ispd18dr` with a toy case `ispd18_sample`:
~~~
$ cd run
$ ./ispd18dr -lef ../toys/ispd18_sample/ispd18_sample.input.lef -def ../toys/ispd18_sample/ispd18_sample.input.def -guide ../toys/ispd18_sample/ispd18_sample.in
put.guide -output ispd18_sample.solution.def -threads 8
~~~

#### Run with a Wrapping Script

Instead of running the binary directly, you may also use a wrapping script `run_clean.py` to save typing and do more:
~~~
$ cd run
$ ./run_clean.py 8s -p ../toys/
~~~

If Innovus® has been properly installed in your OS, an evaluation can be launched by
~~~
$ ./run_clean.py 8s -s eval -p ../toys/
~~~
In the end, a result table will be printed in the terminal.

Furthermore, the solution can be visualized by
~~~
$ ./run_clean.py 8s -s view -p ../toys/
~~~
which gives:

![ispd18_sample.solution.png](/toys/ispd18_sample/ispd18_sample.solution.png)

The three steps, `route`, `eval` and `view` of `run_clean.py` can also be invoked in a single line:
~~~
$ ./run_clean.py 8s -s route eval view -p ../toys/
~~~
More usage about `run_clean.py` can be known by the option `-h`.

### 2.2. Batch Test

The benchmarks can be downloaded from [the hompage of ISPD'18 Contest ](http://www.ispd.cc/contests/18/#benchmarks).
You may let `run_clean.py` know the benchmark path by setting OS environmental variable `BENCHMARK_PATH` or specifying it under option `-p`.
Then,
```
$ cd dr-cu/run
$ ./run.py <benchmark_name...|all> -s route eval [option...]
```

## 3. Modules

* `ispd18eval`: scripts and other files for evaluation, provided by [ISPD'18 Contest](http://www.ispd.cc/contests/18)
* `rsyn`: code from [Rsyn](https://github.com/RsynTeam/rsyn-x) for file IO
* `scripts`: utility python scripts
* `src`: c++ source code
    * `db`: database, including the global grid graph and the net information
    * `single_net`: routing a single net, including querying the global grid graph, building the local grid graph, running maze routing, and some post processing
    * `multi_net`: routing all nets with "rip-up and rereoute" and multithreading
    * `utils`: some utility code
* `toys`: toy test cases


## 4. Results

Experiments are performed on a 64-bit Linux workstation with Intel Xeon Silver 4114 CPU (2.20GHz, 40 cores) and 256GB memory.
Consistent with the contest, eight threads are used.

| design            | WL       | # vias  | og WL   | og # vias | ot WL | ot # vias | ww WL  | short area | # min area | # space | total score  | mem (GB)  | time (s) |
|-------------------|----------|---------|---------|-----------|-------|-----------|--------|------------|------------|---------|--------------|-----------|----------|
| **ispd18_test1**  | 433254   | 32031   | 1706    | 446       | 393   | 0         | 4749   | 0.4        | 0          | 17      | **296504**   | **0.33**  | **11**   |
| **ispd18_test2**  | 7806294  | 317160  | 34194   | 5948      | 4937  | 0         | 44495  | 1.3        | 0          | 73      | **4661740**  | **1.70**  | **85**   |
| **ispd18_test3**  | 8683731  | 307545  | 52408   | 5499      | 5714  | 0         | 45541  | 372.5      | 0          | 161     | **5330014**  | **1.75**  | **113**  |
| **ispd18_test4**  | 26033480 | 658644  | 132938  | 16103     | 9190  | 0         | 59579  | 436.8      | 6          | 1071    | **15304156** | **3.94**  | **320**  |
| **ispd18_test5**  | 27729394 | 916715  | 92872   | 16686     | 1588  | 0         | 44680  | 77.4       | 10         | 496     | **16144832** | **5.42**  | **426**  |
| **ispd18_test6**  | 35595790 | 1403634 | 142595  | 25939     | 8735  | 0         | 69829  | 92.7       | 21         | 587     | **21198243** | **6.48**  | **527**  |
| **ispd18_test7**  | 64994186 | 2271738 | 235497  | 36269     | 16459 | 0         | 106884 | 230.8      | 38         | 325     | **37724327** | **10.77** | **969**  |
| **ispd18_test8**  | 65289434 | 2281513 | 290418  | 38596     | 17082 | 0         | 111173 | 249.5      | 20         | 399     | **37990696** | **11.73** | **1034** |
| **ispd18_test9**  | 54602832 | 2282226 | 284645  | 42078     | 12746 | 0         | 108324 | 162.7      | 28         | 379     | **32592136** | **11.20** | **906**  |
| **ispd18_test10** | 67907614 | 2439531 | 1137257 | 64535     | 30527 | 0         | 197840 | 11370.4    | 44         | 3910    | **47909940** | **11.95** | **1299** |

(WL for "wirelength", og for "out-of-guide", ot for "off-track", ww for "wrong-way")

## 5. License

READ THIS LICENSE AGREEMENT CAREFULLY BEFORE USING THIS PRODUCT. BY USING THIS PRODUCT YOU INDICATE YOUR ACCEPTANCE OF THE TERMS OF THE FOLLOWING AGREEMENT. THESE TERMS APPLY TO YOU AND ANY SUBSEQUENT LICENSEE OF THIS PRODUCT.

License Agreement for Dr. CU

Copyright (c) 2019 by The Chinese University of Hong Kong

All rights reserved

CU-DS LICENSE (adapted from the original BSD license) Redistribution of the any code, with or without modification, are permitted provided that the conditions below are met.

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

Use is limited to non-commercial products only. Users who are interested in using it in commercial products must notify the author and request separate license agreement.

Neither the name nor trademark of the copyright holder or the author may be used to endorse or promote products derived from this software without specific prior written permission.

Users are entirely responsible, to the exclusion of the author, for compliance with (a) regulations set by owners or administrators of employed equipment, (b) licensing terms of any other software, and (c) local, national, and international regulations regarding use, including those regarding import, export, and use of encryption software.

THIS FREE SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR ANY CONTRIBUTOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, EFFECTS OF UNAUTHORIZED OR MALICIOUS NETWORK ACCESS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.