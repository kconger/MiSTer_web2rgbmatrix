#!/bin/bash
#https://github.com/MiSTer-devel/Downloader_MiSTer/blob/main/docs/custom-databases.md

MD5=$(md5sum ../linux/update_web2rgbmatrix.sh | awk '{print $1}')
SIZE=$(ls -o ../linux/update_web2rgbmatrix.sh | awk '{print $4}')
DATE=$(gdate +%s -r ../linux/update_web2rgbmatrix.sh)
cp web2rgbmatrixdb.json-empty web2rgbmatrixdb.json
gsed -i 's/"hash": "XXX"/"hash": "'${MD5}'"/' web2rgbmatrixdb.json
gsed -i 's/"size": XXX/"size": '${SIZE}'/' web2rgbmatrixdb.json
gsed -i 's/"timestamp": XXX/"timestamp": '${DATE}'/' web2rgbmatrixdb.json
mv web2rgbmatrixdb.json ../
