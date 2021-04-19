import os
import shutil
import subprocess
targets = [
    'correlation',
    'covariance',
    'deriche',
    'floyd-warshall',
    'nussinov',
    'adi',
    'fdtd-2d',
    'heat-3d',
    'jacobi-1d',
    'jacobi-2d',
    'seidel-2d',
    'gemm',
    'gemver',
    'gesummv',
    'symm',
    'syr2k',
    'syrk',
    'trmm',
    '2mm',
    '3mm',
    'atax',
    'bicg',
    'doitgen',
    'mvt',
    'cholesky',
    'durbin',
    'gramschmidt',
    'lu',
    'ludcmp',
    'trisolv'
]


terminal_codec = 'utf-8'


def run(target):
    wasm_module_naive = './polybench-c-4.2.1-beta/{}.naive.wasm'.format(target)
    wasm_module_opt = './polybench-c-4.2.1-beta/{}.opt.wasm'.format(target)
    wasm_module_simd = './polybench-c-4.2.1-beta/{}.simd.wasm'.format(target)
    command_naive = ['wasmtime', wasm_module_naive]
    command_opt = ['wasmtime', wasm_module_opt]
    command_simd = ['wasmtime', '--enable-simd', wasm_module_simd]
    proc = subprocess.Popen(
        command_naive, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_naive = proc_out.decode(terminal_codec).replace('\n', '')
    proc = subprocess.Popen(
        command_opt, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_opt = proc_out.decode(terminal_codec).replace('\n', '')
    proc = subprocess.Popen(
        command_simd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_simd = proc_out.decode(terminal_codec).replace('\n', '')
    print("{}, {}, {}, {}".format(target, time_naive, time_opt, time_simd))


if __name__ == '__main__':
    for target in targets:
        run(target)
