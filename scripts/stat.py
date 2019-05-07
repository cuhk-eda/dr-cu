#!/usr/bin/env python3

import argparse, os
from stat_base import *

parser = argparse.ArgumentParser()
parser.add_argument('files', metavar='file', type=str, nargs='+')
args = parser.parse_args()

route_items = ['wire space', 'wire-wire', 'wire-fix', 'via-via', 'via-wire', 'via-fix', 'min area', 'est score', 'mem', 'runtime']
eval_items = ['WL', '# vias', 'og WL', 'og # vias', 'ot WL', 'ot # vias', 'ww WL', 'short area', '# min area', '# spc', '# open', 'tot']

def ParseRoute(file):
    # register
    reRoute = RegexPatternSet()
    # reRoute.Add(['# fails'], '.* ([\d.]+) fail.', False, 'Finish all RRR iterations and PostRoute')  # TODO: clearly print out in log
    reRoute.Add(['wire space', 'wire-wire', 'wire-fix'], '.*SumW[ ]+\| ([\d.]+)[ ]+\| [\d.]+[ ]+\| [\d.]+[ ]+\| ([\d.]+)[ ]+\| ([\d.]+)[ ]+', False, 'Finish all RRR iterations and PostRoute')
    reRoute.Add(['via-via', 'via-wire', 'via-fix'], '.*BigSumV[ ]+\| ([\d.]+)[ ]+\| ([\d.]+)[ ]+\| ([\d.]+)[ ]+', False, 'Finish all RRR iterations and PostRoute')
    reRoute.Add(['min area'], '.*# min-area vio: ([\d.]+)', False, 'Finish all RRR iterations and PostRoute')
    reRoute.Add(['est score'], '.*total score = ([\d.e+]+)', False, 'Finish all RRR iterations and PostRoute')
    reRoute.Add(['mem'], '\tMaximum resident set size \(kbytes\): ([\d]+)')
    reRoute.Add(['runtime'], '\tElapsed \(wall clock\) time \(h:mm:ss or m:ss\): ([:\d.]+)')

    # iterate lines
    result = {}
    with open(file) as f:
        for line in f:
            reRoute.ProcessLine(result, line)
    return result

def ParseEval(file):
    # register
    reRoute = RegexPatternSet()
    reRoute.Add(['WL'], '.*Total wire length \|[ ]+[\d.]+ \|[ ]+([\d.]+)')
    reRoute.Add(['# vias'], '.*Total via count \|[ ]+([\d.]+)')
    reRoute.Add(['og WL'], '.*Out-of-guide wire \|[ ]+[\d.]+ \|[ ]+([\d.]+)')
    reRoute.Add(['og # vias'], '.*Out-of-guide vias \|[ ]+([\d.]+)')
    reRoute.Add(['ot WL'], '.*Off-track wire \|[ ]+[\d.]+ \|[ ]+([\d.]+)')
    reRoute.Add(['ot # vias'], '.*Off-track via \|[ ]+([\d.]+)')
    reRoute.Add(['ww WL'], '.*Wrong-way wire \|[ ]+[\d.]+ \|[ ]+([\d.]+)')
    reRoute.Add(['short area'], '.*Area of metal shorts \|[ ]+[\d.]+ \|[ ]+([\d.]+)')
    reRoute.Add(['# min area'], '.*#min-area violations \|[ ]+([\d.]+)')
    reRoute.Add(['# spc'], '.*#spacing violations \|[ ]+([\d.]+)')
    reRoute.Add(['# open'], '.*#open nets \|[ ]+([\d.]+)')
    reRoute.Add(['tot'], '.*Total Score \|[ ]+([\d.]+)')

    # iterate lines
    result = {}
    with open(file) as f:
        for line in f:
            reRoute.ProcessLine(result, line)
    return result

def is_eval_rpt(file):
    with open(file) as f:
        for line in f:
            if 'Guides and tracks Obedience' in line:
                return True
    return False

route_results = []
eval_results = []
for file in args.files:
    if not os.path.isfile(file):
        print('Warning: file', file, 'does not exist')
        continue
    print('Processing file', file, '...')
    if is_eval_rpt(file):
        eval_results.append(ParseEval(file))
    else:
        route_results.append(ParseRoute(file))
        if 'mem' in route_results[-1]:
            route_results[-1]['mem'] = str(round(float(route_results[-1]['mem']) / 1048576, 6)) # KB to GB

print('ROUTER INFO')
PrintResultTable(route_items, route_results)
print('EVALUATOR INFO')
PrintResultTable(eval_items, eval_results)