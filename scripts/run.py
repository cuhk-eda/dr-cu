#!/usr/bin/env python3

import argparse
import os
import datetime
from run_base import *

# constants
binary = 'ispd18dr'

# all benchmarks
all_benchmarks = Benchmarks()
all_benchmarks.add('ispd18_sample{}', '8s{}', ['', '2', '3'])
all_benchmarks.add('ispd18_test{}', '8t{}', ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10'])
bm_choices = all_benchmarks.get_choices()

# argparse
parser = argparse.ArgumentParser()
parser.add_argument('benchmarks', choices=bm_choices, nargs='+', metavar='BENCHMARK',
                    help='Choices are ' + ', '.join(bm_choices))
parser.add_argument('-m', '--mode', choices=modes)
parser.add_argument('-s', '--steps', choices=['route', 'eval', 'view'], nargs='+', default=['route'])
parser.add_argument('-p', '--benchmark_path')
parser.add_argument('-t', '--threads', default=8)
parser.add_argument('-l', '--log_dir')
args = parser.parse_args()

# seleted benchmarks
bms = all_benchmarks.get_selected(args.benchmarks)
bm_path = args.benchmark_path
if bm_path is None:
    bm_path = os.environ.get('BENCHMARK_PATH')
    if bm_path is None:
        print('Benchmark path is not specified.')
        quit()
    bm_path += '/ispd2018'

# mode cmd_prefix
cmd_prefix = mode_prefixes[args.mode]
if args.mode == 'valgrind':
    print('Please make sure the binary is not compiled with static linking to avoid false alarm')

# run
if args.log_dir is not None:
    log_dir = args.log_dir
else:
    now = datetime.datetime.now()
    log_dir = 'run{:02d}{:02d}'.format(now.month, now.day)
run('mkdir -p {}'.format(log_dir))
print('The following benchmarks will be ran: ', bms)


def route():
    run('/usr/bin/time -v {0} ./{1} -lef {2}.input.lef -def {2}.input.def -guide {2}.input.guide -output {3}.solution.def -threads {4} |& tee {3}.log'.format(
        cmd_prefix, binary, file_name_prefix, bm.full_name, args.threads))
    if args.mode == 'gprof':
        run('gprof {} > {}.gprof'.format(binary, bm.full_name))
        run('./gprof2dot.py -s {0}.gprof | dot -Tpdf -o {0}.pdf'.format(bm.full_name))
    run('mv *.solution.def* *.log *.gprof *.pdf {} 2>/dev/null'.format(bm_log_dir))


def evaluate():
    sol_file = '{}/{}.solution.def'.format(bm_log_dir, bm.full_name)
    log_file = '{}_eval.log'.format(bm.full_name)
    run('cp ispd18eval/ispd18eval* ./')
    run('./ispd18eval.sh -lef {0}.input.lef -guide {0}.input.guide -def {1} -thread {2} | tee {3}'.format(
        file_name_prefix, sol_file, args.threads, log_file))
    run('rm ispd18eval_bin ispd18eval.sh ispd18eval.tcl ispd18eval.w')
    run('mv *.log innovus.* eval.*.rpt *.solution.def.v {} 2>/dev/null'.format(bm_log_dir))


def view():
    def_file = '{}/{}.solution.def'.format(bm_log_dir, bm.full_name)
    lef_file = '{}.input.lef'.format(file_name_prefix)
    file = open("tmp.tcl", "w")
    file.write("loadLefFile {}\n".format(lef_file))
    file.write("loadDefFile {}\n".format(def_file))
    file.write("win\n")
    file.close()
    run('innovus -init tmp.tcl')
    run('rm innovus.* tmp.tcl')


for bm in bms:
    bm_log_dir = '{}/{}'.format(log_dir, bm.abbr_name)
    file_name_prefix = '{0}/{1}/{1}'.format(bm_path, bm.full_name)
    run('mkdir -p {}'.format(bm_log_dir))
    if 'route' in args.steps:
        route()
    if 'eval' in args.steps:
        evaluate()
    if 'view' in args.steps:
        view()