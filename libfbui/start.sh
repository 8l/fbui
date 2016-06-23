#/bin/sh

sync
sync
sync
sync
sync
sync
sync
sync

if [ "$1" == "" ]; then
	export A=4
else
	export A=$1
fi


PanelManager/fbpm -c$A clouds.jpg &
Start/fbstart -c$A &
Clock/fbclock  -c$A -geo72x72+0-0 &
# MailCheck/fbcheck  -geo 150x50+200-0 -c$A &
LoadMonitor/fbload 4 -fg=orange -bg=black -geo200x150-0-0 -c$A &
MPEG/mpeg2decode -$A -b MPEG/2004-18-a-low_mpeg.mpg &
Viewer/fbview  -geo150x120-0+30 Viewer/497_1031_3.tif -c$A &
Scribble/fbscribble -c$A &
Calc/fbcalc -c$A &
Term/fbterm  -c$A &
ToDo/fbtodo -c$A &

#echo "break exit" > cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "break exit" >> cmds
#echo "run -c$A" >> cmds

