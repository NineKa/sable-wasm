import csv
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter
import numpy as np


def parse_number(number_str):
    if number_str.upper() == 'N/A':
        return float('NaN')
    return float(number_str)


def parse_data(path):
    data_file = open(path)
    next(data_file)
    data_reader = csv.reader(data_file)

    target = []
    sablewasm_naive = []
    sablewasm_opt = []
    sablewasm_simd = []
    wasmtime_naive = []
    wasmtime_opt = []
    wasmtime_simd = []
    wasmer_cranelift_naive = []
    wasmer_cranelift_opt = []
    wasmer_cranelift_simd = []
    wasmer_llvm_naive = []
    wasmer_llvm_opt = []
    wasmer_llvm_simd = []

    for row in data_reader:
        target.append(row[0])
        sablewasm_naive.append(parse_number(row[1]))
        sablewasm_opt.append(parse_number(row[2]))
        sablewasm_simd.append(parse_number(row[3]))
        wasmtime_naive.append(parse_number(row[4]))
        wasmtime_opt.append(parse_number(row[5]))
        wasmtime_simd.append(parse_number(row[6]))
        wasmer_cranelift_naive.append(parse_number(row[7]))
        wasmer_cranelift_opt.append(parse_number(row[8]))
        wasmer_cranelift_simd.append(parse_number(row[9]))
        wasmer_llvm_naive.append(parse_number(row[10]))
        wasmer_llvm_opt.append(parse_number(row[11]))
        wasmer_llvm_simd.append(parse_number(row[12]))

    return {
        'target': target,
        'sablewasm_naive': sablewasm_naive,
        'sablewasm_opt': sablewasm_opt,
        'sablewasm_simd': sablewasm_simd,
        'wasmtime_naive': wasmtime_naive,
        'wasmtime_opt': wasmtime_opt,
        'wasmtime_simd': wasmtime_simd,
        'wasmer-cranelift_naive': wasmer_cranelift_naive,
        'wasmer-cranelift_opt': wasmer_cranelift_opt,
        'wasmer-cranelift_simd': wasmer_cranelift_simd,
        'wasmer-llvm_naive': wasmer_llvm_naive,
        'wasmer-llvm_opt': wasmer_llvm_opt,
        'wasmer-llvm_simd': wasmer_llvm_simd
    }


def relative_plt(file_name, x_label, d0, d1, label1 = 'd0') :
    x = np.arange(len(x_label))
    width = 0.618

    fig, ax = plt.subplots()
    axes = plt.gca()
    #axes.set_ylim([0, 2])

    d0 = np.array(d0)
    d1 = np.array(d1)
    d1 = d0/d1

    pl1 = ax.bar(x, d1, width, label=label1)
    ax.axhline(y = 1, linestyle = '--')

    ax.set_xticks(x)
    ax.set_xticklabels(x_label, rotation='vertical')
    ax.legend()

    fig.tight_layout()
    plt.grid(True)
    plt.savefig(file_name)
    plt.close(fig)

def sablewasm_plt(file_name, x_label, d0, d1, d2, label1 = 'd1', label2 = 'd2'):
    x = np.arange(len(x_label))
    width = 0.4

    fig, ax = plt.subplots()
    axes = plt.gca()
    #axes.set_ylim([0, 1.2])

    d0 = np.array(d0)
    d1 = np.array(d1)
    d2 = np.array(d2)
    d1 = d0/d1
    d2 = d0/d2

    pl1 = ax.bar(x - width / 2, d1, width, label = label1)
    pl2 = ax.bar(x + width / 2, d2, width, label = label2)
    ax.axhline(y = 1, linestyle = '--')

    ax.set_xticks(x)
    ax.set_xticklabels(x_label, rotation='vertical')
    ax.legend()

    fig.tight_layout()
    plt.grid(True)
    plt.savefig(file_name)
    plt.close(fig)


polybench = parse_data('SableWasm Benchmark - polybench-c-4.2.1-beta.csv')
for mode in ['naive', 'opt', 'simd']:
    for toolchain in ['wasmtime', 'wasmer-cranelift', 'wasmer-llvm']:
        relative_plt('plots/polybench-relative-{}-{}.pdf'.format(toolchain, mode),
             polybench['target'], 
             polybench['{}_{}'.format(toolchain, mode)],
             polybench['sablewasm_{}'.format(mode)],
             'sablewasm_{}'.format(mode))

sablewasm_plt('plots/polybench.pdf', 
    polybench['target'],
    polybench['sablewasm_naive'], 
    polybench['sablewasm_opt'],
    polybench['sablewasm_simd'],
    'sablewasm_opt', 'sablewasm_simd')

npb = parse_data('SableWasm Benchmark - NPB3.0-omp-C.csv')
for mode in ['naive', 'opt', 'simd']:
    for toolchain in ['wasmtime', 'wasmer-cranelift', 'wasmer-llvm']:
        relative_plt('plots/npb-relative-{}-{}.pdf'.format(toolchain, mode),
             npb['target'], 
             npb['{}_{}'.format(toolchain, mode)],
             npb['sablewasm_{}'.format(mode)],
             'sablewasm_{}'.format(mode))

sablewasm_plt('plots/npb.pdf', 
    npb['target'],
    npb['sablewasm_naive'], 
    npb['sablewasm_opt'],
    npb['sablewasm_simd'],
    'sablewasm_opt', 'sablewasm_simd')

ostrich = parse_data('SableWasm Benchmark - ostrich.csv')
for mode in ['naive', 'opt', 'simd']:
    for toolchain in ['wasmtime', 'wasmer-cranelift', 'wasmer-llvm']:
        relative_plt('plots/ostrich-relative-{}-{}.pdf'.format(toolchain, mode),
             ostrich['target'],
             ostrich['{}_{}'.format(toolchain, mode)],
             ostrich['sablewasm_{}'.format(mode)],
             'sablewasm_{}'.format(mode))

sablewasm_plt('plots/ostrich.pdf', 
    ostrich['target'],
    ostrich['sablewasm_naive'], 
    ostrich['sablewasm_opt'],
    ostrich['sablewasm_simd'],
    'sablewasm_opt', 'sablewasm_simd')