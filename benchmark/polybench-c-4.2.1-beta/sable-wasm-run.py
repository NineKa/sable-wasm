import subprocess

terminal_codec = 'utf-8'


def run(target_name):
    source = target_name
    object_file = target_name + '.o'
    sable_file = target_name + '.sable'

    compile_process = subprocess.run([
        '../../build/sable-wasm', '--unsafe', '--opt',
        source,
        '-o', object_file])
    if compile_process.returncode != 0:
        return 'N/A'

    link_proces = subprocess.run(
        ['ld', '-shared', object_file, '-o', sable_file])
    if link_proces.returncode != 0:
        return 'N/A'

    tester_process = subprocess.Popen([
        '../../build/tester', sable_file],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()

    if len(proc_error) != 0:
        return 'N/A'
    return proc_out.decode(terminal_codec).replace('\n', '')


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

for target in targets:
    naive_result = run(target + '.naive.wasm')
    opt_result = run(target + '.opt.wasm')
    simd_result = run(target + '.simd.wasm')

    print('{}, {}, {}, {}'.format(target, naive_result, opt_result, simd_result))
