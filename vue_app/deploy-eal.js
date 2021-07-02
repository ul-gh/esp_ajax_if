#!/usr/bin/env node
// deploy-eal.js: Deploy ESP-AJAX-Lab vue.js application
// by copying only the .gz-compressed vue-cli build artifacts
// into the SPIFFS data folder of the main source tree
//
// U. Lukas 2021-02-24

const fs = require('fs');
const copyfiles = require('copyfiles');
const { exec } = require("child_process");
const process = require('process');

let project_dir = "..";
let data_dir = `${project_dir}/data/www`;
if (!fs.existsSync(data_dir)) {
    project_dir = ".";
    data_dir = `${project_dir}/data/www`;
}
if (!fs.existsSync(data_dir)) {
    console.error("Error: Did not find data folder: " + data_dir);
    process.exit(1);
}

const build_dir = `${project_dir}/build`;

const spiffs_file = `${build_dir}/spiffs.bin`;
const index_html_file = "./dist/index.html.gz";
const src_path_glob = "./dist/**/*";

function copy_to_data_folder() {
    // We want to deploy the .gz compressed versions only, so the
    // original, uncompressed build files are excluded
    options = {
        exclude: ["./**/*.js", "./**/*.css", "./**/*.html",
		  "./**/*.svg", "./**/*.json", "./**/*.map"
	],
        up: 1,
        follow: true,
        verbose: true,
        error: true,
    };
    console.log("Copying from: " + src_path_glob + " to: " + data_dir);
    copyfiles([src_path_glob, data_dir], options, console.error);
}

function err_exec(cmd_str) {
    exec(cmd_str, (error, stdout, stderr) => {
        if (error) {
            console.error(error.message);
            return;
        }
        if (stderr) {
            console.error(`stderr: ${stderr}`);
            return;
        }
        console.log(`stdout: ${stdout}`);
    });
}

function make_spiffs() {
    try {
    if (fs.existsSync(index_html_file)) {
        copy_to_data_folder();
        err_exec(`idf.py --project-dir ${project_dir} --build-dir ${build_dir} spiffs_spiffs_bin`)
    } else {
        console.error("Error: Did not find index.html at: " + index_html_file);
    }
    } catch(err) {
    console.error(err);
    }
}

function flash() {
    try {
    if (fs.existsSync(spiffs_file)) {
        copy_to_data_folder();
        err_exec(`idf.py --project-dir ${project_dir} --build-dir ${build_dir} spiffs-flash`)
    } else {
        console.error("Error: Did not find SPIFFS filesystem image at: " + spiffs_file);
    }
    } catch(err) {
    console.error(err);
    }
}


if (process.argv.includes("make_spiffs")) {
    make_spiffs();
}

if (process.argv.includes("flash_spiffs")) {
    flash();
}

if (process.argv.includes("deploy_data")) {
    copy_to_data_folder();
}