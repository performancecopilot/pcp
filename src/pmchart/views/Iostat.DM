#kmchart
version 1

global width 600
global height 300
global points 360

tab "Device Mapper TRAFFIC"
chart title "DM READ thruput [%h]" style stacking
	plot color #-cycle host * metric disk.dm.read_bytes

chart title "DM WRITE thruput [%h]" style stacking
	plot color #-cycle host * metric disk.dm.write_bytes

chart title "DM READ OPS [%h]" style stacking
	plot color #-cycle host * metric disk.dm.read

chart title "DM WRITE OPS [%h]" style stacking
	plot color #-cycle host * metric disk.dm.write

tab "Device Mapper IOSTAT"
chart title "DM Avg Wait Time (msecs / req) [%h]" style line
	plot color #-cycle host * metric disk.dm.await

chart title "DM Avg Req Size (Kbytes / req) [%h]" style line
	plot color #-cycle host * metric disk.dm.avg_rqsz

chart title "DM Avg Queue Len [%h]" style line
	plot color #-cycle host * metric disk.dm.avg_qlen

chart title "DM Avg %Util [%h]" style line scale from 0 to 100
	plot color #-cycle host * metric disk.dm.util
