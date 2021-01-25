#!/bin/sh
set -e

npm run build

index_html_full_path="dist/index.html"
if [ ! -f dist/index.html ]; then
    echo "Did not find $index_html_full_path. Abort."
    exit 1
fi

rsync -vrptgo \
    --exclude=*.map \
    dist/* ../src/data/www/

