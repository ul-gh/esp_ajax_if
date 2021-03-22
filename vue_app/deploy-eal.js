#!/usr/bin/env node
// deploy-eal.js: Deploy ESP-AJAX-Lab vue.js application
// by copying only the .gz-compressed vue-cli build artifacts
// into the SPIFFS data folder of the main source tree
//
// U. Lukas 2021-02-24

const fs = require('fs');
const copyfiles = require('copyfiles');

const index_html_file = "./dist/index.html.gz";
const src_path_glob = "./dist/**/*";
const dest_path_glob = "../src/data/www/";

function deploy_files() {
    // We want to deploy the .gz compressed versions only, so the
    // original, uncompressed build files are excluded
    options = {
        exclude: ["./**/*.js", "./**/*.css", "./**/*.html", "./**/*.svg", "./**/*.json", "./**/*.map"],
        up: 1,
        follow: true,
        verbose: true,
        error: true,
    };
    console.log("Copying from: " + src_path_glob + " to: " + dest_path_glob);
    copyfiles([src_path_glob, dest_path_glob], options, console.error);
}


try {
  if (fs.existsSync(index_html_file)) {
      deploy_files();
  } else {
    console.error("Error: Did not find index.html at: " + index_html_file);
  }
} catch(err) {
  console.error(err);
}

