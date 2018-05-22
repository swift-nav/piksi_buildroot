#[macro_use]
extern crate hyper;
extern crate reqwest;
extern crate uuid;
#[macro_use]
extern crate serde_derive;
extern crate clap;

use uuid::Uuid;
use std::process::Command;
use std::io;
use std::io::{BufReader, Write};
use std::fs::File;
use std::path::Path;
use std::iter::Iterator;
use clap::{Arg, App};

header! { (DeviceUid, "Device-Uid") => [Uuid] }
header! { (CurrentVersion, "Current-Version") => [String] }

#[derive(Deserialize, Debug)]
struct OTAServiceResponse {
    version: String,
    sha256: String,
    url: String,
}

fn sha256(path: &std::path::Path) -> Result<String, String> {
    // Linux
    let mut base_cmd = Command::new("sha256sum");
    // OS X:
    // let mut base_cmd = Command::new("shasum");
    //         base_cmd.args(&["-a", "256"]);
    let hash_cmd_output = String::from_utf8(
                            base_cmd
                            .arg(path.to_str().ok_or("Path error")?)
                            .output()
                            .map_err(|e| format!("Error calling shasum: {}", e.to_string()))?
                            .stdout)
                          .map_err(|e| format!("Error encoding shasum ouput: {}", e.to_string()))?;
    return Ok(String::from(hash_cmd_output
                           .split(" ")
                           .next()
                           .ok_or("Error parsing shasum output")?));
}

fn upgrade_firmware(path: &std::path::Path) -> Result<(), String> {
    println!("performing upgrade");
    let cmd_output = String::from_utf8(
                        Command::new("upgrade_tool")
                        .arg(path.to_str().ok_or("Path error")?)
                        .output()
                        .map_err(|e| format!("Error calling upgrade_tool: {}", e.to_string()))?
                        .stdout)
                     .map_err(|e| format!("Error encoding upgrade_tool output: {}", e.to_string()))?;
    println!("{}", cmd_output);
    return Ok(());
}

fn is_uuid(uuid: String) -> Result<(), String> {
    match Uuid::parse_str(&uuid) {
        Err(_) => Err(String::from("Device UUID not correctly formatted")),
        Ok(_) => Ok(())
    }
}

fn ota_service_request(uuid: Uuid, current_version: &str, url: &str) -> Result<OTAServiceResponse, String> {
    let mut response = reqwest::Client::new()
        .get(url)
        .header(DeviceUid(uuid))
        .header(CurrentVersion(String::from(current_version)))
        .send().map_err(|e| format!("Error making request to OTA service: {}", e))?;
    if !response.status().is_success() {
        return Err(format!("OTA service request error, server responded {}", response.status()));
    }
    return response.json().map_err(|e| format!("Error parsing JSON response: {}", e));
}

fn main() {
    // Parse command line arguments
    let matches = App::new("ota_tool")
        .about("Over-the-Air Update (OTA) Tool. Checks for new firmware and installs it.")
        .author("Swift Navigation")
        .arg(Arg::with_name("UUID")
                 .help("Device UUID")
                 .required(true)
                 .index(1)
                 .validator(is_uuid))
        .arg(Arg::with_name("VERSION")
                 .help("Current Firmware Version")
                 .required(true)
                 .index(2))
        .arg(Arg::with_name("url")
                 .short("u")
                 .long("url")
                 .value_name("URL")
                 .help("Service endpoint URL")
                 .takes_value(true))
        .get_matches();

    // OTA service endpoint URL
    let url = matches.value_of("url")
                     .unwrap_or("https://upgrader.skylark.swiftnav.com/images");

    // OTA request parameters
    let current_version = matches.value_of("VERSION")
                                 .expect("Error retrieving command line argument VERSION");
    let uuid = Uuid::parse_str(matches.value_of("UUID").unwrap())
                    .expect("Error parsing UUID");

    println!("endpoint url: {}", url);
    println!("device uuid: {}", uuid);
    println!("current version: {}", current_version);

    // Make a request to the OTA service to get the details of what firmware we
    // should be running
    let mut decoded = ota_service_request(uuid, current_version, url).expect("OTA service request error");

    println!("target version: {}", decoded.version);

    if decoded.version == current_version {
      println!("Already up-to-date");
      return;
    }

    println!("url: {}", decoded.url);
    println!("expected sha256: {}", decoded.sha256);

    let mut fw_req = BufReader::new(reqwest::get(&decoded.url).expect("Error downloading firmware"));
    let fw_path = Path::new("/tmp/PiksiMulti.bin");
    let mut fw_file = File::create(fw_path).expect("Error opening temporary firmware file");
    io::copy(&mut fw_req, &mut fw_file).expect("Error writing firmware file");
    fw_file.flush().expect("Error flushing firmware file");

    println!("downloaded path: {}", fw_path.display());

    let hash = sha256(fw_path).unwrap();

    println!("downloaded sha256: {}", hash);

    if decoded.sha256 != hash {
      println!("Error: downloaded firmware sha256 doesn't match");
      return;
    }

    upgrade_firmware(fw_path).unwrap();
}
