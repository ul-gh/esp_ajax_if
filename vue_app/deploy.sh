#!/bin/sh
set -e

index_html_full_path="dist/index.html.gz"
if [ ! -f "$index_html_full_path" ]; then
    echo "Did not find $index_html_full_path. Abort."
    exit 1
fi

# .js, .css, .html, .svg and .json static content is compressed, we only want
# the -.gz versions deployed to the web server.
rsync -vrptgo \
    --exclude "*.js" \
    --exclude "*.css" \
    --exclude "*.html" \
    --exclude "*.svg" \
    --exclude "*.json" \
    dist/* ../src/data/www/

