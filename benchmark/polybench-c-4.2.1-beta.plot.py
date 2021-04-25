import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

book = pd.ExcelFile('polybench-c-4.2.1-beta.xlsx')
book_content = {sheet_name: book.parse(sheet_name)
                for sheet_name in book.sheet_names}


def parse_sheet(sheet_name):
    sheet_content = book_content[str(sheet_name)]
    return {
        'name': sheet_content['Name'].tolist(),
        'sablewasm_naive': sheet_content['SableWasm(Naive)'].to_numpy(),
        'sablewasm_opt': sheet_content['SableWasm(Opt)'].to_numpy(),
        'sablewasm_simd': sheet_content['SableWasm(SIMD)'].to_numpy(),
        'wasmtime_naive': sheet_content['Wasmtime(Naive)'].to_numpy(),
        'wasmtime_opt': sheet_content['Wasmtime(Opt)'].to_numpy(),
        'wasmtime_simd': sheet_content['Wasmtime(SIMD)'].to_numpy(),
        'wasmer-cranelift_naive': sheet_content['Wasmer-Cranelift(Naive)'].to_numpy(),
        'wasmer-cranelift_opt': sheet_content['Wasmer-Cranelift(Opt)'].to_numpy(),
        'wasmer-cranelift_simd': sheet_content['Wasmer-Cranelift(SIMD)'].to_numpy(),
        'wasmer-llvm_naive': sheet_content['Wasmer-LLVM(Naive)'].to_numpy(),
        'wasmer-llvm_opt': sheet_content['Wasmer-LLVM(Opt)'].to_numpy(),
        'wasmer-llvm_simd': sheet_content['Wasmer-LLVM(SIMD)'].to_numpy()
    }


def compute_relative_speedup(sheet_contents):
    assert(all([sheet_content['name'] == sheet_contents[0]['name']
                for sheet_content in sheet_contents]))

    result = {}

    for toolchain in ['wasmtime', 'wasmer-cranelift', 'wasmer-llvm']:
        for mode in ['naive', 'opt', 'simd']:
            sablewasm_prefs = [sheet_content['sablewasm_{}'.format(mode)]
                               for sheet_content in sheet_contents]
            toolchain_prefs = [sheet_content['{}_{}'.format(toolchain, mode)]
                               for sheet_content in sheet_contents]
            speed_up = [toolchain_pref / sablewasm_pref
                        for toolchain_pref, sablewasm_pref in
                        zip(toolchain_prefs, sablewasm_prefs)]
            result[(toolchain, mode)] = speed_up

    return result


def compute_speedup_average(speedups):
    result = {}
    for key in speedups.keys():
        result[key] = np.average(speedups[key], axis=0)
    return result


def compute_speedup_min(speedups):
    result = {}
    for key in speedups.keys():
        result[key] = np.min(speedups[key], axis=0)
    return result


def compute_speedup_max(speedups):
    result = {}
    for key in speedups.keys():
        result[key] = np.max(speedups[key], axis=0)
    return result


def relative_plt(file_name, x_label,
                 speedup_average, speedup_min, speedup_max):

    x = np.arange(len(x_label))
    width = 0.618

    fig, ax = plt.subplots()
    axes = plt.gca()
    axes.set_ylim([0, max(2, max(speedup_max) + 0.1)])

    error_high = speedup_max - speedup_average
    error_low = speedup_average - speedup_min
    yerr = np.array([error_low, error_high])
    print(yerr)

    pl1 = ax.bar(x, speedup_average, width, yerr=yerr,
                 capsize=4, error_kw={'linewidth': 1})

    ax.axhline(y=1, linestyle='--')

    ax.set_xticks(x)
    ax.set_xticklabels(x_label, rotation='vertical')

    fig.tight_layout()
    plt.grid(linestyle='--')
    plt.savefig(file_name)
    plt.close(fig)


sheet_contents = [parse_sheet(sheet_name) for sheet_name in range(10)]
names = sheet_contents[0]['name']
speedups = compute_relative_speedup(sheet_contents)
speedup_averages = compute_speedup_average(speedups)
speedup_mins = compute_speedup_min(speedups)
speedup_maxs = compute_speedup_max(speedups)

for toolchain, mode in speedups.keys():
    filename = 'plots/ploybench-{}-{}.pdf'.format(toolchain, mode)
    speedup_average = speedup_averages[(toolchain, mode)]
    speedup_min = speedup_mins[(toolchain, mode)]
    speedup_max = speedup_maxs[(toolchain, mode)]
    relative_plt(filename, names,
                 speedup_average, speedup_min, speedup_max)
