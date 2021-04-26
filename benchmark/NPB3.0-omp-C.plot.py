import plot

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
        speedup_percentile_10 = speedup_percentile_10s[(toolchain, mode)]
        speedup_percentile_90 = speedup_percentile_90s[(toolchain, mode)]
        plot.relative_plt(filename, names,
                          speedup_average, speedup_percentile_10, speedup_percentile_90,
                          capsize=15)
