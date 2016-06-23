#!/bin/sh

if [ $UID != 0 ]
then echo "You must be root to install!"; exit
fi

DIRS="Clock ToDo Term LoadMonitor Viewer MailCheck Dump WindowManager PanelManager"

for DIR in $DIRS
do
cd $DIR
make install
cd ..
done


