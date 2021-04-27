import subprocess
import re

terminal_codec = 'utf-8'

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


def decode_time(proc_out):
    try:
        out = proc_out.decode(terminal_codec)
        return out
    except UnicodeDecodeError:
        return '#N/A'


def sablewasm_run(target_name):
    proc = subprocess.Popen('./run.sh {}'.format(target_name),
                            shell=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL)
    proc.wait()
    if proc.returncode != 0:
        return '#N/A'
    proc_stdout, proc_stderr = proc.communicate()
    return decode_time(proc_stdout)


def wasmtime_run(target_name, flags=''):
    proc = subprocess.Popen(
        'wasmtime {} {}'.format(flags, target_name),
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL)
    proc.wait()
    if proc.returncode != 0:
        return '#N/A'
    proc_stdout, proc_stderr = proc.communicate()
    return decode_time(proc_stdout)


def wasmer_cranelift_run(target_name, flags=''):
    proc = subprocess.Popen(
        'wasmer run --cranelift {} {}'.format(flags, target_name),
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL)
    proc.wait()
    if proc.returncode != 0:
        return '#N/A'
    proc_stdout, proc_stderr = proc.communicate()
    return decode_time(proc_stdout)


def wasmer_llvm_run(target_name, flags=''):
    proc = subprocess.Popen(
        'wasmer run --llvm {} {}'.format(flags, target_name),
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL)
    proc.wait()
    if proc.returncode != 0:
        return '#N/A'
    proc_stdout, proc_stderr = proc.communicate()
    return decode_time(proc_stdout)


for target in targets:
    print('{},{},{},{},{},{},{},{},{},{},{},{},{}'.format(
        target,
        sablewasm_run('{}.naive.wasm'.format(target)),
        sablewasm_run('{}.opt.wasm'.format(target)),
        sablewasm_run('{}.simd.wasm'.format(target)),
        wasmtime_run('{}.naive.wasm'.format(target)),
        wasmtime_run('{}.opt.wasm'.format(target)),
        wasmtime_run('{}.simd.wasm'.format(target), '--enable-simd'),
        wasmer_cranelift_run('{}.naive.wasm'.format(target)),
        wasmer_cranelift_run('{}.opt.wasm'.format(target)),
        wasmer_cranelift_run('{}.simd.wasm'.format(target), '--enable-simd'),
        wasmer_llvm_run('{}.naive.wasm'.format(target)),
        wasmer_llvm_run('{}.opt.wasm'.format(target)),
        wasmer_llvm_run('{}.simd.wasm'.format(target), '--enable-simd')))
