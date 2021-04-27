import subprocess
import re

terminal_codec = 'utf-8'
time_regex = r'Time in seconds = *([0-9]*.[0-9]*)'

targets = [
    'BT.A',
    'CG.A',
    'EP.A',
    'FT.A',
    'IS.A',
    'LU.W',
    'MG.A',
    'SP.A',
]


def decode_time(proc_out):
    try:
        out = proc_out.decode(terminal_codec)
        matches = re.search(time_regex, out)
        return matches.group(1)
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
