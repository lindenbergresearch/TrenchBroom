#!/bin/zsh

DATE=$(date "+%Y-%m%d-%H%M%S-%s")

tar -czvf ${DATE}_backup_tb.tgz ./common/ ./app/ ./lib/ ./*.*