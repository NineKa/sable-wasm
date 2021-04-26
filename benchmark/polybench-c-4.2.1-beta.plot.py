import plot

if __name__ == '__main__':
    workbook_path = 'polybench-c-4.2.1-beta.xlsx'
    workbook = plot.parse_workbook(workbook_path)

    sheet_contents = [plot.parse_sheet(workbook, sheet_name) for sheet_name in range(10)]

    names = sheet_contents[0]['name']

    speedups = plot.compute_relative_speedup(sheet_contents)
    speedup_averages = plot.compute_speedup_average(speedups)
    speedup_mins = plot.compute_speedup_min(speedups)
    speedup_maxs = plot.compute_speedup_max(speedups)

    for toolchain, mode in speedups.keys():
        print('plotting relative speed up for {}-{}'.format(toolchain, mode))
        filename = 'plots/polybench-{}-{}.pdf'.format(toolchain, mode)
        speedup_average = speedup_averages[(toolchain, mode)]
        speedup_min = speedup_mins[(toolchain, mode)]
        speedup_max = speedup_maxs[(toolchain, mode)]
        plot.relative_plt(filename, names,
                          speedup_average, speedup_min, speedup_max)
