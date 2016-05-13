#set terminal X11
set terminal postscript eps enhanced color 20
set output "latency.eps"
#set title "PingPong Latency (small messages)"
set xlabel "Message Size (Bytes)"
set ylabel "Latency (microseconds)"
set pointsize 1.25
set logscale x 2
set logscale y 10
#set xrange [1:5200]
set xrange [1:4194304]
#set yrange [0:]
set yrange [1:]
#set key bottom right
set key top left

#set xtics ( "0" 0, "1" 1, "2" 2, "4" 4, "8" 8, "16" 16, "32" 32, "64" 64, "128" 128, "256" 256, "512" 512, "1K" 1024, "2K" 2048, "4K" 4096)

#set xtics ( "1" 1, "4" 4, "16" 16, "64" 64, "256" 256, "1K" 1024, "4K" 4096)

set style line  1 lt 1 linecolor rgb "black"   lw 5
set style line  2 lt 1 linecolor rgb "red"     lw 5
set style line  3 lt 1 linecolor rgb "orange"  lw 5
set style line  4 lt 1 linecolor rgb "yellow"  lw 5
set style line  5 lt 1 linecolor rgb "green"   lw 5
set style line  6 lt 1 linecolor rgb "#00FFFF" lw 5
set style line  7 lt 1 linecolor rgb "blue"    lw 5
set style line  8 lt 1 linecolor rgb "purple"  lw 5
set style line  9 lt 1 linecolor rgb "#4B0082" lw 5
set style line 10 lt 1 linecolor rgb "#7F7F7F" lw 5
set style line 11 lt 1 linecolor rgb "gray"    lw 5

plot "native_linux_rdma.txt" using 1:2     title  "Native Linux RDMA" with linespoints ls 1, \
     "client_linux_rdma.txt" using 1:2     title  "Proxy Client Linux RDMA" with linespoints ls 2
