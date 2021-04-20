import csv
import matplotlib
import matplotlib.pyplot as plt
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


def relative_plt(file_name, x_label, d0, d1, label0 = 'd0') :
    x = np.arange(len(x_label))
    width = 0.618

    fig, ax = plt.subplots()
    axes = plt.gca()
    axes.set_ylim([0, 2])

    d0 = np.array(d0)
    d1 = np.array(d1)
    d0 = d0 / d1

    pl0 = ax.bar(x, d0, width, label = label0)
    ax.axhline(y = 1, linestyle = '--')

    ax.set_xticks(x)
    ax.set_xticklabels(x_label, rotation='vertical')
    ax.legend()

    fig.tight_layout()
    plt.savefig(file_name)
    plt.close(fig)

polybench = parse_data('SableWasm Benchmark - polybench-c-4.2.1-beta.csv')
for mode in ['naive', 'opt', 'simd']:
    for toolchain in ['wasmtime', 'wasmer-cranelift', 'wasmer-llvm']:
        relative_plt('plots/polybench-relative-{}-{}.pdf'.format(toolchain, mode),
             polybench['target'], polybench['sablewasm_{}'.format(mode)],
             polybench['{}_{}'.format(toolchain, mode)],
             'sablewasm_{}'.format(mode))

npb = parse_data('SableWasm Benchmark - NPB3.0-omp-C.csv')
for mode in ['naive', 'opt', 'simd']:
    for toolchain in ['wasmtime', 'wasmer-cranelift', 'wasmer-llvm']:
        relative_plt('plots/npb-relative-{}-{}.pdf'.format(toolchain, mode),
             npb['target'], npb['sablewasm_{}'.format(mode)],
             npb['{}_{}'.format(toolchain, mode)],
             'sablewasm_{}'.format(mode))

ostrich = parse_data('SableWasm Benchmark - ostrich.csv')
for mode in ['naive', 'opt', 'simd']:
    for toolchain in ['wasmtime', 'wasmer-cranelift', 'wasmer-llvm']:
        relative_plt('plots/ostrich-relative-{}-{}.pdf'.format(toolchain, mode),
             ostrich['target'], ostrich['sablewasm_{}'.format(mode)],
             ostrich['{}_{}'.format(toolchain, mode)],
             'sablewasm_{}'.format(mode))