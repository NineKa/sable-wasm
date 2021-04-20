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
        'wasmer_cranelift_naive': wasmer_cranelift_naive,
        'wasmer_cranelift_opt': wasmer_cranelift_opt,
        'wasmer_cranelift_simd': wasmer_cranelift_simd,
        'wasmer_llvm_naive': wasmer_llvm_naive,
        'wasmer_llvm_opt': wasmer_llvm_opt,
        'wasmer_llvm_simd': wasmer_llvm_simd
    }


def group_plt(x_label, d0, d1, d2, d3, label0='d0', label1='d1',  label2='d2', label3='d3'):
    labels = np.arange(len(x_label))
    width = 0.35

    fig, ax = plt.subplots()
    pl0 = ax.bar(labels - width / 4, d0, width, label=label0)
    pl1 = ax.bar(labels - width / 4, d1, width, label=label1)
    pl2 = ax.bar(labels - width / 4, d2, width, label=label2)
    pl3 = ax.bar(labels - width / 4, d3, width, label=label3)

    fig.tight_layout()
    plt.show()


polybench = parse_data('SableWasm Benchmark - polybench-c-4.2.1-beta.csv')

group_plt(polybench['target'],
          polybench['sablewasm_naive'],
          polybench['wasmtime_naive'],
          polybench['wasmer_cranelift_naive'],
          polybench['wasmer_llvm_naive'])
