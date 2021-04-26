import subprocess
import os
import re

terminal_codec = 'utf-8'

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


def run(target_name):
    source = target_name
    object_file = target_name + '.o'
    sable_file = target_name + '.sable'

    compile_process = subprocess.run([
        '../../build/sable-wasm', '--unsafe', '--opt',
        source,
        '-o', object_file],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL)
    if compile_process.returncode != 0:
        return '#N/A'

    link_proces = subprocess.run(
        ['ld', '-shared', object_file, '-o', sable_file],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL)
    if link_proces.returncode != 0:
        os.remove(object_file)
        return '#N/A'

    tester_process = subprocess.Popen([
        '../../build/tester', sable_file],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = tester_process.communicate()

    if len(proc_error) != 0:
        os.remove(object_file)
        os.remove(sable_file)
        return '#N/A'

    os.remove(object_file)
    os.remove(sable_file)

    try:
        out = proc_out.decode(terminal_codec).replace('\n', '')
        regex = r'Time in seconds = *([0-9]*.[0-9]*)'
        matches = re.search(regex, out)
        if matches is None:
            return '#N/A'
        return matches.group(1)
    except UnicodeDecodeError:
        return '#N/A'


for target in targets:
    naive_result = run(target + '.naive.wasm')
    opt_result = run(target + '.opt.wasm')
    simd_result = run(target + '.simd.wasm')

    print('{},{},{},{}'.format(target, naive_result, opt_result, simd_result))
