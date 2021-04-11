targets = [
    "aead_aes256gcm",
    "aead_aes256gcm2",
    "aead_chacha20poly1305",
    "aead_chacha20poly13052",
    "aead_xchacha20poly1305",
    "auth",
    "auth2",
    "auth3",
    "auth5",
    "auth6",
    "auth7",
    "box",
    "box2",
    "box7",
    "box8",
    "box_easy",
    "box_easy2",
    "box_seal",
    "box_seed",
    "chacha20",
    "codecs",
    "core1",
    "core2",
    "core3",
    "core4",
    "core5",
    "core6",
    "core_ed25519",
    "core_ristretto255",
    "ed25519_convert",
    "generichash",
    "generichash2",
    "generichash3",
    "hash",
    "hash3",
    "kdf",
    "keygen",
    "kx",
    "metamorphic",
    "misuse",
    "onetimeauth",
    "onetimeauth2",
    "onetimeauth7",
    "pwhash_argon2i",
    "pwhash_argon2id",
    "pwhash_scrypt",
    "pwhash_scrypt_ll",
    "randombytes",
    "scalarmult",
    "scalarmult2",
    "scalarmult5",
    "scalarmult6",
    "scalarmult7",
    "scalarmult8",
    "scalarmult_ed25519",
    "scalarmult_ristretto255",
    "secretbox",
    "secretbox2",
    "secretbox7",
    "secretbox8",
    "secretbox_easy",
    "secretbox_easy2",
    "secretstream",
    "shorthash",
    "sign",
    "siphashx24",
    "sodium_core",
    "sodium_utils",
    "sodium_utils2",
    "sodium_utils3",
    "sodium_version",
    "stream",
    "stream2",
    "stream3",
    "stream4",
    "verify1",
    "xchacha20"
]

import subprocess, shutil, os

terminal_codec = 'utf-8'


def run_wasmtime(target, opt):
    wasm_module = './libsodium-1.0.18/{}.{}.wasm'.format(target, opt)
    command = ['wasmtime', wasm_module]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    return proc_out.decode(terminal_codec).replace('\n', '')


def run_wasmer_cranelift(target, opt):
    wasm_module = './libsodium-1.0.18/{}.{}.wasm'.format(target, opt)
    command = ['wasmer', 'run', '--cranelift', wasm_module]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    return proc_out.decode(terminal_codec).replace('\n', '')


def run_wasmer_llvm(target, opt):
    wasm_module = './libsodium-1.0.18/{}.{}.wasm'.format(target, opt)
    command = ['wasmer', 'run', '--llvm', wasm_module]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    return proc_out.decode(terminal_codec).replace('\n', '')


def run_sable_wasm(target, opt):
    wasm_module = './libsodium-1.0.18/{}.{}.wasm'.format(target, opt)
    o_file = './libsodium-1.0.18/{}.{}.o'.format(target, opt)
    sable_file = './libsodium-1.0.18/{}.{}.sable'.format(target, opt)

    sable_wasm_cmd = ['../build/sable-wasm', '--unsafe', '--opt', wasm_module, '-o', o_file]
    subprocess.Popen(sable_wasm_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE).wait()
    ld_cmd = ['ld', '-shared', o_file, '-o', sable_file]
    subprocess.Popen(ld_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE).wait()
    tester_cmd = ['../build/tester', sable_file]
    proc = subprocess.Popen(tester_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc_out, proc_error = proc.communicate()
    os.remove(o_file)
    os.remove(sable_file)
    if len(proc_error) != 0:
        return ''
    return proc_out.decode(terminal_codec).replace('\n', '')


if __name__ == '__main__':
    for target in targets:
        wasmtime = run_wasmtime(target, 'naive')
        wasmer_cranelift = run_wasmer_cranelift(target, 'naive')
        wasmer_llvm = run_wasmer_llvm(target, 'naive')
        sable_wasm = run_sable_wasm(target, 'naive')
        wasmtime = 'N/A' if len(wasmtime) == 0 else wasmtime
        wasmer_cranelift = 'N/A' if len(wasmer_cranelift) == 0 else wasmer_cranelift
        wasmer_llvm = 'N/A' if len(wasmer_llvm) == 0 else wasmer_llvm
        sable_wasm = 'N/A' if len(sable_wasm) == 0 else sable_wasm
        print('{},{},{},{},{}'.format(target, wasmtime, wasmer_cranelift, wasmer_llvm, sable_wasm))
