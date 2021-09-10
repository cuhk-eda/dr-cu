#!/usr/bin/env python3

import argparse, os

all_targets = ['ispd19dr']
run_files = 'scripts/*.py ispd18eval ispd19eval'

def run(command):
    if args.print_commands:
        print(command)
    if os.system(command) != 0:
        if not args.print_commands:
            print(command)
        quit()

# cmake & copy run files
mode_cmake_options = {
    None                : '',
    'debug'             : '-DCMAKE_BUILD_TYPE=Debug',
    'release'           : '-DCMAKE_BUILD_TYPE=Release',
    'profile'           : '-DCMAKE_CXX_FLAGS=-pg',
    'release_profile'   : '-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-pg',
    'asan'              : '-DCMAKE_CXX_FLAGS=-fsanitize=address',
    'debug_asan'        : '-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-fsanitize=address',
    'tsan'              : '-DCMAKE_CXX_FLAGS=-fsanitize=thread',
    'debug_tsan'        : '-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-fsanitize=thread'
}

# argparse
parser = argparse.ArgumentParser(description='Build ispd19dr in Linux')
parser.add_argument('-t', '--targets', choices=all_targets, nargs='+')
parser.add_argument('-o', '--mode', choices=mode_cmake_options.keys())
parser.add_argument('-c', '--cmake_options', default='')
parser.add_argument('-m', '--make_options', default='-j 6')
parser.add_argument('-u', '--unittest', action='store_true')
parser.add_argument('-p', '--print_commands', action='store_true')
parser.add_argument('-b', '--build_dir', default='build')
parser.add_argument('-r', '--run_dir', default='run')
args = parser.parse_args()

# targets
if args.unittest:
    build_targets = ['unittest_salt']
else:
    build_targets = args.targets

run(f'cmake src -B{args.build_dir} {mode_cmake_options[args.mode]} {args.cmake_options}')
run(f'mkdir -p {args.run_dir}')
run(f'cp -u -R {run_files} {args.run_dir}')

# make
if build_targets:
    for target in build_targets:
        run(f'cmake --build {args.build_dir} --target {target} -- {args.make_options}')
else:
    run(f'cmake --build {args.build_dir} -- {args.make_options}')
cp_targets = all_targets if not build_targets else build_targets
for target in cp_targets:
    run('cp -u {}/{} {}'.format(args.build_dir, target, args.run_dir))

# unit test
if args.unittest:
    root_dir = os.path.curdir
    os.chdir(args.run_dir)
    run('./{}'.format('unittest_salt'))
    os.chdir(root_dir)
