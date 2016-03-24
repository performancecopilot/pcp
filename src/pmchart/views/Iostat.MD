#kmchart
version 1

global width 600
global height 300
global points 360

tab "Multiple Devices - TRAFFIC"
chart title "DM READ thruput [%h]" style stacking
	plot color #-cycle host * metric disk.md.read_bytes

chart title "DM WRITE thruput [%h]" style stacking
	plot color #-cycle host * metric disk.md.write_bytes

chart title "DM READ OPS [%h]" style stacking
	plot color #-cycle host * metric disk.md.read

chart title "DM WRITE OPS [%h]" style stacking
	plot color #-cycle host * metric disk.md.write

tab "Multiple Devices - IOSTAT"
chart title "DM Avg Wait Time (msecs / req) [%h]" style line
	plot color #-cycle host * metric disk.md.await

chart title "DM Avg Req Size (Kbytes / req) [%h]" style line
	plot color #-cycle host * metric disk.md.avg_rqsz

chart title "DM Avg Queue Len [%h]" style line
	plot color #-cycle host * metric disk.md.avg_qlen

chart title "DM Avg %Util [%h]" style line scale from 0 to 100
	plot color #-cycle host * metric disk.md.util
