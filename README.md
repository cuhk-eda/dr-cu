# Dr.CU

Dr. CU is a VLSI detailed routing tool developed by the research team supervised by Prof. Evangeline F.Y. Young in The Chinese University of Hong Kong (CUHK). Different from global routing, detailed routing takes care of many detailed design rules and is performed on a significantly larger routing grid graph. In advanced technology nodes, it becomes the most complicated and time-consuming stage in the VLSI physical design flow. To tackle the challenges, we design and implement several efficient and effective data structures and algorithms under a holistic framework:

* A set of two-level sparse data structures
* An optimal correct-by-construction path search
* An efficient bulk synchronous parallel scheme
* ...

More details are in the following papers:

* Gengjie Chen, Chak-Wa Pui, Haocheng Li, Jingsong Chen, Bentian Jiang, and Evangeline F.Y. Young,
["Detailed Routing by Sparse Grid Graph and Minimum-Area-Captured Path Search"](https://doi.org/10.1145/3287624.3287678),
IEEE/ACM Asia and South Pacific Design Automation Conference (ASPDAC), Tokyo, Japan, Jan 21-24, 2019.
* Gengjie Chen, Chak-Wa Pui, Haocheng Li, and Evangeline F.Y. Young,
["Dr. CU: Detailed Routing by Sparse Grid Graph and Minimum-Area-Captured Path Search"](https://doi.org/10.1109/TCAD.2019.2927542),
IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems (TCAD), vol. 39, no. 9, pp. 1902–1915, 2020.
* Haocheng Li, Gengjie Chen, Bentian Jiang, Jingsong Chen, and Evangeline F.Y. Young,
["Dr. CU 2.0: A Scalable Detailed Routing Framework with Correct-by-Construction Design Rule Satisfaction"](https://doi.org/10.1109/ICCAD45719.2019.8942074),
IEEE/ACM International Conference on Computer-Aided Design (ICCAD), Westminster, CO, USA, Nov 4-7, 2019.

(Dr. CU supports [ISPD'18 benchmarks](http://www.ispd.cc/contests/18/#benchmarks)
and [ISPD'19 benchmarks](http://www.ispd.cc/contests/19/#benchmarks).
This version of code is consistent with the ICCAD paper.)

## 0. Config

Enter the following commands when **first time** using Dr.CU:

```bash
$ cd config
$ chmod 700 config.sh
$ ./config.sh
```

## 1. How to Build

**Step 1:** Download the source code. For example,
```bash
$ git clone https://github.com/cuhk-eda/dr-cu
```

**Step 2:** Go to the project root and build by
```bash
$ cd dr-cu
$ scripts/build.py -o release -t ispd19dr
```

**Note from @cptsai: It seems that it will fail when setting `-t ispd18dr`.**

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
```bash
$ cd run
$ ./ispd18dr -lef ../toys/ispd18_sample/ispd18_sample.input.lef -def ../toys/ispd18_sample/ispd18_sample.input.def -guide ../toys/ispd18_sample/ispd18_sample.in
put.guide -output ispd18_sample.solution.def -threads 8
```

#### Run with a Wrapping Script

Instead of running the binary directly, you may also use a wrapping script `run.py` to save typing and do more:
~~~
$ cd run
$ ./run.py 8s -p ../toys/
~~~

If Innovus® has been properly installed in your OS, an evaluation can be launched by
~~~
$ ./run.py 8s -s eval -p ../toys/
~~~
In the end, a result table will be printed in the terminal.

Furthermore, the solution can be visualized by
~~~
$ ./run.py 8s -s view -p ../toys/
~~~
which gives:

![ispd18_sample.solution.png](/toys/ispd2018/ispd18_sample/ispd18_sample.solution.png)

The three steps, `route`, `eval` and `view` of `run.py` can also be invoked in a single line:
~~~
$ ./run.py 8s -s route eval view -p ../toys/
~~~
More usage about `run.py` can be known by the option `-h`.

### 2.2. Batch Test

The benchmarks can be downloaded from [the hompage of ISPD'18 Contest ](http://www.ispd.cc/contests/18/#benchmarks).
You may let `run.py` know the benchmark path by setting OS environmental variable `BENCHMARK_PATH` or specifying it under option `-p`.
Then,
```
$ cd run
$ ./run.py <benchmark_name...|all> -s route eval [option...]
```

## 3. Modules

* `ispd18eval`: scripts and other files for evaluation, provided by [ISPD'18 Contest](http://www.ispd.cc/contests/18)
* `ispd19eval`: scripts and other files for evaluation, provided by [ISPD'19 Contest](http://www.ispd.cc/contests/19)
* `rsyn`: code from [Rsyn](https://github.com/RsynTeam/rsyn-x) for file IO
* `scripts`: utility python scripts
* `src`: C++ source code
    * `db`: database, including the global grid graph and the net information
    * `single_net`: routing a single net, including querying the global grid graph, building the local grid graph, running maze routing, and some post processing
    * `multi_net`: routing all nets with "rip-up and rereoute" and multithreading
    * `utils`: some utility code
* `toys`: toy test cases


## 4. Results

Experiments are performed on a 64-bit Linux workstation with Intel Xeon Silver 4114 CPU (2.20GHz, 40 cores) and 256GB memory.
Consistent with the contest, eight threads are used.

|  design         | WL           | #sv      | og WL      | og #v  | ot WL     | ot #v | ww WL      | #short | short a | #min a | #prl  | #eol | #cut | #adj | #cnr  | #spc | #o |  total score  |  mem (GB) |  time (s) |
|-----------------|--------------|----------|------------|--------|-----------|-------|------------|--------|---------|--------|-------|------|------|------|-------|------|----|---------------|-----------|-----------|
| `ispd18_test1`  | 433546.30    | 32402    | 2178.88    | 443    | 414.10    | 0     | 5832.07    | N/A    | 0.11    | 0      | N/A   | N/A  | N/A  | N/A  | N/A   | 2    | 0  | **291291**    | **0.38**  | **14**    |
| `ispd18_test2`  | 7832686.75   | 325684   | 41335.35   | 6542   | 5292.68   | 0     | 54137.45   | N/A    | 0.12    | 0      | N/A   | N/A  | N/A  | N/A  | N/A   | 57   | 0  | **4700934**   | **1.83**  | **124**   |
| `ispd18_test3`  | 8717807.31   | 318309   | 74908.60   | 6636   | 5941.85   | 0     | 60458.50   | N/A    | 222.50  | 0      | N/A   | N/A  | N/A  | N/A  | N/A   | 94   | 0  | **5298745**   | **1.99**  | **205**   |
| `ispd18_test4`  | 26420290.64  | 729312   | 378137.43  | 29895  | 17444.76  | 0     | 205213.27  | N/A    | 245.33  | 93     | N/A   | N/A  | N/A  | N/A  | N/A   | 593  | 0  | **15756404**  | **9.47**  | **1200**  |
| `ispd18_test5`  | 27801300.46  | 965544   | 150150.75  | 21868  | 5826.66   | 3     | 77342.76   | N/A    | 62.45   | 141    | N/A   | N/A  | N/A  | N/A  | N/A   | 362  | 0  | **16366743**  | **7.06**  | **637**   |
| `ispd18_test6`  | 35703509.13  | 1480617  | 243107.40  | 36581  | 16482.90  | 16    | 119011.40  | N/A    | 14.40   | 265    | N/A   | N/A  | N/A  | N/A  | N/A   | 542  | 0  | **21630646**  | **6.58**  | **710**   |
| `ispd18_test7`  | 65173410.39  | 2402543  | 403550.30  | 55532  | 32564.19  | 0     | 186466.07  | N/A    | 164.14  | 357    | N/A   | N/A  | N/A  | N/A  | N/A   | 196  | 0  | **38412191**  | **12.70** | **1413**  |
| `ispd18_test8`  | 65469076.07  | 2412121  | 404288.83  | 54369  | 32394.17  | 0     | 182631.92  | N/A    | 179.39  | 401    | N/A   | N/A  | N/A  | N/A  | N/A   | 192  | 0  | **38602461**  | **13.54** | **1411**  |
| `ispd18_test9`  | 54760288.72  | 2410790  | 358490.45  | 55318  | 26357.22  | 0     | 177587.80  | N/A    | 14.83   | 522    | N/A   | N/A  | N/A  | N/A  | N/A   | 109  | 0  | **33129214**  | **11.30** | **1122**  |
| `ispd18_test10` | 68090186.48  | 2594386  | 1066700.56 | 97600  | 39856.21  | 0     | 236109.72  | N/A    | 2582.10 | 600    | N/A   | N/A  | N/A  | N/A  | N/A   | 918  | 0  | **42704252**  | **11.82** | **2224**  |
| `ispd19_test1`  | 642583.70    | 36797    | 13538.58   | 1567   | 762.59    | 744   | 11284.83   | 55     | 19.82   | 17     | 10    | 22   | 19   | 5    | 55    | N/A  | 0  | **597403**    | **1.28**  | **150**   |
| `ispd19_test2`  | 24961335.76  | 811080   | 395795.89  | 34271  | 22002.73  | 23615 | 209929.36  | 1639   | 547.08  | 634    | 965   | 987  | 172  | 94   | 5984  | N/A  | 0  | **21910640**  | **13.81** | **1694**  |
| `ispd19_test3`  | 842169.62    | 65501    | 12447.07   | 1672   | 1938.80   | 662   | 16236.76   | 74     | 22.92   | 49     | 201   | 125  | 32   | 78   | 108   | N/A  | 0  | **1060034**   | **0.97**  | **62**    |
| `ispd19_test4`  | 30491193.70  | 1031333  | 568882.56  | 45247  | 15201.72  | 3     | 122369.64  | 2264   | 1783.13 | 37     | 264   | 47   | 0    | 0    | 0     | N/A  | 0  | **22312599**  | **12.09** | **1745**  |
| `ispd19_test5`  | 4780469.72   | 153504   | 14036.38   | 2688   | 2087.56   | 15    | 15212.36   | 287    | 70.18   | 3      | 144   | 16   | 0    | 0    | 0     | N/A  | 0  | **3297338**   | **1.75**  | **155**   |
| `ispd19_test6`  | 66066591.23  | 1998487  | 760296.22  | 66862  | 25717.70  | 12468 | 457307.57  | 4022   | 1558.64 | 754    | 1175  | 822  | 377  | 56   | 1235  | N/A  | 0  | **47336856**  | **10.42** | **2836**  |
| `ispd19_test7`  | 122558107.84 | 4833913  | 883097.59  | 102010 | 45009.73  | 24305 | 607014.34  | 2423   | 899.00  | 1916   | 21733 | 1682 | 753  | 48   | 3512  | N/A  | 0  | **98736640**  | **20.82** | **7165**  |
| `ispd19_test8`  | 188472592.09 | 7365292  | 1225631.51 | 163218 | 72500.42  | 36390 | 729298.28  | 3048   | 1070.81 | 2830   | 4292  | 2793 | 1266 | 81   | 5903  | N/A  | 0  | **136530155** | **30.96** | **10714** |
| `ispd19_test9`  | 285390772.80 | 12249476 | 1881593.79 | 276221 | 125462.74 | 60624 | 1213996.98 | 6646   | 1933.66 | 5010   | 6815  | 5157 | 3484 | 133  | 9484  | N/A  | 0  | **214519790** | **49.40** | **15010** |
| `ispd19_test10` | 282178218.27 | 12544541 | 1821064.92 | 255274 | 127318.93 | 60621 | 1412830.92 | 6879   | 1984.04 | 4855   | 6769  | 4872 | 2990 | 136  | 10429 | N/A  | 0  | **214337744** | **50.72** | **15932** |

(WL for "wirelength", v for "via", sv for "single-cut via", og for "out-of-guide", ot for "off-track", ww for "wrong-way", a for "area", prl for "parallel-run-length spacing", eol for "end-of-line spacing")

## 5. License

READ THIS LICENSE AGREEMENT CAREFULLY BEFORE USING THIS PRODUCT. BY USING THIS PRODUCT YOU INDICATE YOUR ACCEPTANCE OF THE TERMS OF THE FOLLOWING AGREEMENT. THESE TERMS APPLY TO YOU AND ANY SUBSEQUENT LICENSEE OF THIS PRODUCT.



License Agreement for Dr. CU



Copyright (c) 2019-2021, The Chinese University of Hong Kong



All rights reserved.



CU-SD LICENSE (adapted from the original BSD license) Redistribution of the any code, with or without modification, are permitted provided that the conditions below are met. 



1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.



2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.



3. Neither the name nor trademark of the copyright holder or the author may be used to endorse or promote products derived from this software without specific prior written permission.



4. Users are entirely responsible, to the exclusion of the author, for compliance with (a) regulations set by owners or administrators of employed equipment, (b) licensing terms of any other software, and (c) local, national, and international regulations regarding use, including those regarding import, export, and use of encryption software.



THIS FREE SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR ANY CONTRIBUTOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, EFFECTS OF UNAUTHORIZED OR MALICIOUS NETWORK ACCESS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
