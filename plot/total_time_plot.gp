reset
set title 'Runtime comparison' 
set xlabel 'F(n)'
set ylabel 'time(nsec)'
set term png enhanced font 'Verdana,10'
set output 'timeplot.png'
set grid

plot [0:][0:] \
'./original' using 1:2 with linespoints linewidth 2 title 'iterative', \
'./fast_doubling' using 1:2 with linespoints linewidth 2 title 'fast doubling'