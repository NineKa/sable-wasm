import plot, numpy

if __name__ == '__main__':
    workbook_path = 'NPB3.0-omp-C.xlsx'
    workbook = plot.parse_workbook(workbook_path)

    sheet_contents = [plot.parse_sheet(workbook, sheet_name) for sheet_name in range(10)]

    names = sheet_contents[0]['name']

    speedups = plot.compute_relative_speedup(sheet_contents)
    speedup_averages = plot.compute_speedup_average(speedups)
    speedup_percentile_10s = plot.compute_speedup_percentile(speedups, 10)
    speedup_percentile_90s = plot.compute_speedup_percentile(speedups, 90)

    for toolchain, mode in speedups.keys():
        print('plotting relative speed up for {}-{}'.format(toolchain, mode))
        filename = 'plots/npb-{}-{}.pdf'.format(toolchain, mode)
        speedup_average = speedup_averages[(toolchain, mode)]
        print(numpy.nanmean(speedup_average))
        speedup_percentile_10 = speedup_percentile_10s[(toolchain, mode)]
        speedup_percentile_90 = speedup_percentile_90s[(toolchain, mode)]
        plot.relative_plt(filename, names,
                          speedup_average, speedup_percentile_10, speedup_percentile_90,
                          capsize=15)

    speedup_opt = plot.compute_relative_speedup_opt(sheet_contents)
    speedup_opt_averages = plot.compute_speedup_average_opt(speedup_opt)
    speedup_opt_percentile_10 = plot.compute_speedup_percentile_opt(speedup_opt, 10)
    speedup_opt_percentile_90 = plot.compute_speedup_percentile_opt(speedup_opt, 90)
    plot.relative_plt('npb-opt-speedup.pdf', names,
                      speedup_opt_averages, speedup_opt_percentile_10, speedup_opt_percentile_90, capsize=15)
    
    speedup_simd = plot.compute_relative_speedup_simd(sheet_contents)
    speedup_simd_averages = plot.compute_speedup_average_opt(speedup_simd)
    speedup_simd_percentile_10 = plot.compute_speedup_percentile_opt(speedup_simd, 10)
    speedup_simd_percentile_90 = plot.compute_speedup_percentile_opt(speedup_simd, 90)
    plot.relative_plt('npb-simd-speedup.pdf', names,
                      speedup_simd_averages, speedup_simd_percentile_10, speedup_simd_percentile_90, capsize=15)