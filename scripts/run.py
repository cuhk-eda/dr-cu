#!/usr/bin/env python3

import argparse
import os
import datetime
from run_base import *

# constants
binary = 'ispd19dr'

# argparse
parser = argparse.ArgumentParser()
parser.add_argument('benchmarks', choices=all_benchmarks.get_choices(), nargs='+', metavar='BENCHMARK',
                    help='Choices are ' + ', '.join(all_benchmarks.get_choices()))
parser.add_argument('-s', '--steps', choices=['route', 'eval', 'view'], nargs='+', default=['route'])
parser.add_argument('-p', '--benchmark_path')
parser.add_argument('-t', '--threads', type=int, default=8)
args = parser.parse_args()

# seleted benchmarks
bms = all_benchmarks.get_selected(args.benchmarks)
bm_path = args.benchmark_path
if bm_path is None:
    bm_path = os.environ.get('BENCHMARK_PATH')
    if bm_path is None:
        print('Benchmark path is not specified.')
        quit()


# run
if True:
    now = datetime.datetime.now()
    log_dir = 'run{:02d}{:02d}'.format(now.month, now.day)

run('mkdir -p {}'.format(log_dir))
print('The following benchmarks will be ran: ', bms)


def route():
    run('/usr/bin/time -v ./{0} -lef {1}.input.lef -def {1}.input.def {2} -threads {3} -tat 2000000000 -output {4}.solution.def |& tee {4}.log'.format(
        binary, file_name_prefix, guide_opt, args.threads, bm.full_name))

    run('mv *.solution.def* *.log *.gprof *.pdf {} 2>/dev/null'.format(bm_log_dir))


def evaluate():
    sol_file = '{0}/{1}.solution.def'.format(bm_log_dir, bm.full_name)
    log_file = '{}_eval.log'.format(bm.full_name)
    bm_yy = int(bm.full_name[4:6])
    run('cp ispd{0}eval/ispd{0}eval* ./'.format(bm_yy))

    def evaluate_once():
        if bm_yy == 18:
            run('./ispd{0}eval.sh -lef {1}.input.lef {2} -def {3} -thread {4} |& tee {5}'.format(
                bm_yy, file_name_prefix, guide_opt, sol_file, args.threads, log_file))
        else:
            run('./ispd{0}eval.sh -lef {1}.input.lef {2} -idef {1}.input.def -odef {3} -thread {4} |& tee {5}'.format(
                bm_yy, file_name_prefix, guide_opt, sol_file, args.threads, log_file))

    evaluate_once()

    run('rm ispd{0}eval_bin ispd{0}eval.sh ispd{0}eval.tcl ispd{0}eval.w *.def.v eval.def'.format(bm_yy))
    run('mv *.log innovus.* eval.*.rpt {} 2>/dev/null'.format(bm_log_dir))


def view():
    file = open("tmp.tcl", "w")
    file.write("loadLefFile {}.input.lef\n".format(file_name_prefix))
    file.write("loadDefFile {}/{}.solution.def\n".format(bm_log_dir, bm.full_name))
    file.write("setMultiCpuUsage -localCpu {}\n".format(args.threads))
    file.write("setDrawView place\n")
    file.write("win\n")
    file.close()
    run('innovus -init tmp.tcl')
    run('rm innovus.* *.drc.rpt *.solution.def.v tmp.tcl')


for bm in bms:
    bm_log_dir = '{}/{}'.format(log_dir, bm.abbr_name)
    file_name_prefix = '{0}/ispd20{1}/{2}/{2}'.format(bm_path, bm.full_name[4:6], bm.full_name)
    if bm.abbr_name in ['9t1', '9t2', '9t3', '9t6']:
        guide_opt = '-guide {0}.guide'.format(file_name_prefix)
    else:
        guide_opt = '-guide {0}.input.guide'.format(file_name_prefix)

    run('mkdir -p {}'.format(bm_log_dir))
    if 'route' in args.steps:
        route()
    if 'eval' in args.steps:
        evaluate()
    if 'view' in args.steps:
        view()
