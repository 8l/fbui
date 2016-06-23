#/bin/sh

if [ "$1" == "" ]; then
	export A=4
else
	export A=$1
fi

Clock/fbclock  -c$A -geo130x50+0-0 &
MailCheck/fbcheck  -geo 150x50+200-0 -c$A &
 LoadMonitor/fbload 4  -geo80x150-0-0 -c$A &
Viewer/fbview  -geo80x120-0+30 Viewer/meander.jpg -c$A &
 Term/fbterm  -c$A &
Calc/fbcalc -geo80x200-0+160 -c$A &

sleep 2
# sleep needed because fbwm doesn't handle requests for autoplacement
WindowManager/fbwm clouds.jpg -c$A 

#sleep 60
#Dump/fbdump
