#/bin/sh

DIRS=". Calc Launcher MPEG Test Clock ToDo Term LoadMonitor Viewer
MailCheck Dump WindowManager PanelManager"

for DIR in $DIRS
do
cd $DIR
make clean
if [ "$DIR" != "." ]
then cd ..
fi
done
