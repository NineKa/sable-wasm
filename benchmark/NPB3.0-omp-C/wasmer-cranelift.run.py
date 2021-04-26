import subprocess
import re

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


terminal_codec = 'utf-8'


def get_time(out_bytes):
    try:
        out = out_bytes.decode(terminal_codec).replace('\n', '')
        regex = r'Time in seconds = *([0-9]*.[0-9]*)'
        matches = re.search(regex, out)
        if matches is None:
            return '#N/A'
        return matches.group(1)
    except UnicodeDecodeError:
        return "#N/A"


def run(target):
    wasm_module_naive = '{}.naive.wasm'.format(target)
    wasm_module_opt = '{}.opt.wasm'.format(target)
    wasm_module_simd = '{}.simd.wasm'.format(target)
    command_naive = ['wasmer', 'run', '--cranelift', wasm_module_naive]
    command_opt = ['wasmer', 'run', '--cranelift', wasm_module_opt]
    command_simd = ['wasmer', 'run', '--cranelift',
                    '--enable-simd', wasm_module_simd]
    proc = subprocess.Popen(
        command_naive, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_naive = get_time(proc_out)
    proc = subprocess.Popen(
        command_opt, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_opt = get_time(proc_out)
    proc = subprocess.Popen(
        command_simd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    time_simd = get_time(proc_out)
    print("{},{},{},{}".format(target, time_naive, time_opt, time_simd))


if __name__ == '__main__':
    for target in targets:
        run(target)
