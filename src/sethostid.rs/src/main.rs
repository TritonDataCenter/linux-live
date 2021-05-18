/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright 2021 Joyent, Inc.
 */

/*
 * This program sets the hostid based on a crc32 of the system's UUID string.
 * Normally, a Linux machine sets the hostid based on the system's IP address or
 * based on a random number if the IP addess is not known.
 *
 * This is useful on live media that would like to import a zfs pool so that
 * pool-in-use detection doesn't get tripped up across reboots.
 */

use std::env;
use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::process::Command;

extern crate crc;
use crc::{crc32, Hasher32};

static UUIDSTRLEN: usize = 36;

fn main() {
    let args: Vec<String> = env::args().collect();
    let dmidecode = "/usr/sbin/dmidecode";
    let dmidecode_args = ["-s", "system-uuid"];

    let mut crc = crc32::Digest::new(crc32::IEEE);
    let mut path = Path::new("/etc/hostid");

    if args.len() == 2 {
        path = Path::new(&args[1]);
    } else if args.len() != 1 {
        eprintln!("Usage: sethostid [file]");
        std::process::exit(1);
    }

    if path.exists() {
        eprintln!("sethostid: {} exsts", path.display());
        std::process::exit(1);
    }

    let output = Command::new(&dmidecode)
        .args(&dmidecode_args)
        .output()
        .expect("Failed to get system-uuid");

    if output.stdout.len() != UUIDSTRLEN + 1 {
        eprintln!(
            "sethostid: got {} bytes from dmidecode, expected {}",
            output.stdout.len(),
            UUIDSTRLEN + 1
        );
        std::process::exit(1);
    }

    crc.write(&output.stdout[0..UUIDSTRLEN]);

    let mut file = match File::create(&path) {
        Err(why) => {
            eprintln!("couldn't create {}: {}", path.display(), why);
            std::process::exit(1);
        },
        Ok(file) => file,
    };

    match file.write_all(&crc.sum32().to_le_bytes()) {
        Err(why) => {
            eprintln!("couldn't write to {}: {}", path.display(), why);
            std::process::exit(1);
        },
        Ok(_) => (),
    }
}
