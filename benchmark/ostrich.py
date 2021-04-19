import re
import os
import shutil
import subprocess

targets = [
    'nqueens',
    'crc',
    'lud',
    'needle',
    'bfs',
    'hmm',
    'pagerank',
    'lavamd',
    'spmv',
    'fft',
    'srad',
    'backprop'
]


terminal_codec = 'utf-8'


def run(target):
    wasm_module_naive = './ostrich/{}.naive.wasm'.format(target)
    wasm_module_opt = './ostrich/{}.opt.wasm'.format(target)
    wasm_module_simd = './ostrich/{}.simd.wasm'.format(target)
    command_naive = ['wasmtime', wasm_module_naive]
    command_opt = ['wasmtime', wasm_module_opt]
    command_simd = ['wasmtime', '--enable-simd', wasm_module_simd]
    proc = subprocess.Popen(
        command_naive, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_naive = proc_out.decode(
        terminal_codec).replace('\n', '')
    proc = subprocess.Popen(
        command_opt, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_opt = proc_out.decode(terminal_codec).replace('\n', '')
    proc = subprocess.Popen(
        command_simd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_simd = proc_out.decode(terminal_codec).replace('\n', '')
    print("{}\nnaive: {}\nopt:   {}\nsimd:  {}\n".format(
        target, time_naive, time_opt, time_simd))


if __name__ == '__main__':
    for target in targets:
        run(target)
