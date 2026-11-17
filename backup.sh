#!/bin/zsh

DATE=$(date "+%Y%m%d%H%M%S-%s")

tar -czvf ${DATE}_backup_tb.tgz ./common ./app ./lib ./*.sh ./.cl*


#tar --exclude='ordner/unterordner1' --exclude='ordner/unterordner2' -czvf archiv.tar.gz ordner/
