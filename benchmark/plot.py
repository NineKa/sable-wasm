import pandas as pd
import matplotlib.pyplot as plt
import numpy as np


def parse_sheet(workbook, sheet_name):
    sheet_content = workbook[str(sheet_name)]
    result = {}

    def convert_as_list(key_name, column_name):
        result[key_name] = sheet_content[column_name].tolist()

    def convert_as_numpy_array(key_name, column_name):
        result[key_name] = sheet_content[column_name].to_numpy()

    convert_as_list('name', 'Name')
    convert_as_numpy_array('sablewasm_naive', 'SableWasm(Naive)')
    convert_as_numpy_array('sablewasm_opt', 'SableWasm(Opt)')
    convert_as_numpy_array('sablewasm_simd', 'SableWasm(SIMD)')
    convert_as_numpy_array('wasmtime_naive', 'Wasmtime(Naive)')
    convert_as_numpy_array('wasmtime_opt', 'Wasmtime(Opt)')
    convert_as_numpy_array('wasmtime_simd', 'Wasmtime(SIMD)')
    convert_as_numpy_array('wasmer-cranelift_naive', 'Wasmer-Cranelift(Naive)')
    convert_as_numpy_array('wasmer-cranelift_opt', 'Wasmer-Cranelift(Opt)')
    convert_as_numpy_array('wasmer-cranelift_simd', 'Wasmer-Cranelift(SIMD)')
    convert_as_numpy_array('wasmer-llvm_naive', 'Wasmer-LLVM(Naive)')
    convert_as_numpy_array('wasmer-llvm_opt', 'Wasmer-LLVM(Opt)')
    convert_as_numpy_array('wasmer-llvm_simd', 'Wasmer-LLVM(SIMD)')

    return result


def parse_workbook(workbook_path):
    workbook = pd.ExcelFile(workbook_path)
    workbook_content = {sheet_name: workbook.parse(sheet_name)
                        for sheet_name in workbook.sheet_names}
    return workbook_content


def compute_relative_speedup(sheet_contents):
    assert (all([sheet_content['name'] == sheet_contents[0]['name']
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


def compute_speedup_percentile(speedups, ratio):
    result = {}
    for key in speedups.keys():
        result[key] = np.percentile(speedups[key], ratio, axis=0)
    return result


def relative_plt(file_name, x_label,
                 speedup_average, speedup_lower_error, speedup_upper_error):
    x = np.arange(len(x_label))
    width = 0.618

    fig, ax = plt.subplots()
    axes = plt.gca()
    axes.set_ylim([0, max(2, max(speedup_upper_error) + 0.1)])

    error_high = speedup_upper_error - speedup_average
    error_low = speedup_average - speedup_lower_error
    yerr = np.array([error_low, error_high])

    ax.bar(x, speedup_average, width, yerr=yerr, capsize=4, error_kw={'linewidth': 1})

    ax.axhline(y=1, linestyle='--')

    ax.set_xticks(x)
    ax.set_xticklabels(x_label, rotation='vertical')

    fig.tight_layout()
    plt.grid(linestyle='--')
    plt.savefig(file_name)
    plt.close(fig)
