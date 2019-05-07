import os

# run command
def run(command):
    print(command)
    os.system('/bin/bash -c \'{0}\''.format(command))

# run modes
modes = ['gdb', 'valgrind', 'vgdb', 'gprof']
mode_prefixes = {
    None        : '',
    'gprof'     : '',
    'gdb'       : 'gdb --args',
    'valgrind'  : 'valgrind',
    'vgdb'      : 'valgrind --vgdb-error=0'
}

# benchmarks
class Benchmark:
    def __init__(self, full_name, abbr_name):
        self.full_name = full_name
        self.abbr_name = abbr_name
    def __repr__(self):
        return self.full_name

class Benchmarks:
    def __init__(self):
        self.__bms = []

    def add(self, full_name_pat, abbr_name_pat, ids):
        for id in ids:
            self.__bms.append(Benchmark(full_name_pat.format(id), abbr_name_pat.format(id)))

    def get_choices(self):
        choices = []
        for bm in self.__bms:
            choices.append(bm.full_name)
            choices.append(bm.abbr_name)
        choices.append('all')
        return choices

    def get_selected(self, names):
        if 'all' in names:
            return self.__bms
        else:
            selected = []
            for name in names:
                for bm in self.__bms:
                    if name == bm.abbr_name or name == bm.full_name:
                        selected.append(bm)
                        break
            return selected
