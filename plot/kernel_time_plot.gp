reset
set title 'Runtime(kernel) comparison' 
set xlabel 'F(n)'
set ylabel 'time(nsec)'
set term png enhanced font 'Verdana,10'
set output 'timeplot(kernel).png'
set grid

plot [0:][0:] \
'./original_k' using 2:3 with linespoints linewidth 2 title 'iterative', \
'./fast_doubling_k' using 2:3 with linespoints linewidth 3 title 'fast doubling'