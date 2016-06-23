#/bin/sh

FILES="libfbui.a libfbui.o Calc/fbcalc Launcher/fblauncher
Clock/fbclock Term/fbterm LoadMonitor/fbload Viewer/fbview
MailCheck/fbcheck Test/fbtest WindowManager/fbwm PanelManager/fbpm
ToDo/fbtodo MPEG/mpeg2decode"

DIRS=". Calc Launcher MPEG Test Clock ToDo Term LoadMonitor Viewer
MailCheck Dump WindowManager PanelManager"

for FILE in $FILES
do
rm -f $FILE
done

for DIR in $DIRS
do
cd $DIR
make
if [ "$DIR" != "." ]
then cd ..
fi
done
