pathes = {
  'correlation'    : 'datamining/correlation',
  'covariance'     : 'datamining/covariance',
  'deriche'        : 'medley/deriche',
  'floyd-warshall' : 'medley/floyd-warshall',
  'nussinov'       : 'medley/nussinov',
  'adi'            : 'stencils/adi',
  'fdtd-2d'        : 'stencils/fdtd-2d',
  'heat-3d'        : 'stencils/heat-3d',
  'jacobi-1d'      : 'stencils/jacobi-1d',
  'jacobi-2d'      : 'stencils/jacobi-2d',
  'seidel-2d'      : 'stencils/seidel-2d',
  'gemm'           : 'linear-algebra/blas/gemm',
  'gemver'         : 'linear-algebra/blas/gemver',
  'gesummv'        : 'linear-algebra/blas/gesummv',
  'symm'           : 'linear-algebra/blas/symm',
  'syr2k'          : 'linear-algebra/blas/syr2k',
  'syrk'           : 'linear-algebra/blas/syrk',
  'trmm'           : 'linear-algebra/blas/trmm',
  '2mm'            : 'linear-algebra/kernels/2mm',
  '3mm'            : 'linear-algebra/kernels/3mm',
  'atax'           : 'linear-algebra/kernels/atax',
  'bicg'           : 'linear-algebra/kernels/bicg',
  'doitgen'        : 'linear-algebra/kernels/doitgen',
  'mvt'            : 'linear-algebra/kernels/mvt',
  'cholesky'       : 'linear-algebra/solvers/cholesky',
  'durbin'         : 'linear-algebra/solvers/durbin',
  'gramschmidt'    : 'linear-algebra/solvers/gramschmidt',
  'lu'             : 'linear-algebra/solvers/lu',
  'ludcmp'         : 'linear-algebra/solvers/ludcmp',
  'trisolv'        : 'linear-algebra/solvers/trisolv'
}

benchmark_root = None
copy_to_dir = None
suffix = None

import os

for name, path in pathes.items():
  os.chdir(benchmark_root + '/' + path)
  os.system('make clean')
  os.system('make')
  os.system('cp {}/{}/{} {}/{}/{}.{}.wasm'.format(
    benchmark_root, path, name,
    copy_to_dir, path, name, suffix
  ))