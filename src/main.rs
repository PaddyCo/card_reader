extern crate hidapi;

use std::thread;
use std::time;
use std::fs::File;
use std::io::prelude::*;
use std::io;
use hidapi::HidApi;

struct InsertCardAction {
    player_number: u8,
    card_id: [u8; 8],
}

struct RemoveCardAction {
    player_number: u8,
}


fn main() {
    let api = HidApi::new().unwrap();

    // TODO: Reserve PID (http://pid.codes/1209/)
    let (vid, pid) = (0x1209, 0x4AD1);

    // Find the specific device's first interface (In our case this is the RawHID interface)
    match api.devices().iter().find(|d| d.vendor_id == vid && d.product_id == pid && d.interface_number == 0) {
        Some(device) => {
            open_device(&api, device);
        },
        None => {
            eprintln!("Could not find compatible device.");
            thread::sleep(time::Duration::from_millis(1000));
        }
    }
}

fn open_device(api: &hidapi::HidApi, device_info: &hidapi::HidDeviceInfo) {
    loop {
        // Connect to device using its VID and PID
        println!("Attempting to connect to device...");
        match device_info.open_device(&api) {
            Ok(device) => {
                println!("Device connected!");
                match device.get_manufacturer_string() {
                    Ok(manufacturer) => println!("Manufacturer: {}", manufacturer.unwrap()),
                    _ => {}
                }
                match device.get_product_string() {
                    Ok(product) => println!("Product: {}", product.unwrap()),
                    _ => {}
                }
                device_poll(&device);
            },
            Err(e) => {
                eprintln!("Error: {}", e);
                thread::sleep(time::Duration::from_millis(1000));
            }
        };
    }
}

fn device_poll(device: &hidapi::HidDevice) {
    loop {
        println!("Waiting for data...");

        // Read data from device
        let mut buf = [0u8; 64];

        match device.read(&mut buf[..]) {
            Ok(_result) => {
                if buf[0] != 0xEA && buf[1] != 0xCA {
                    println!("Incorrect header, ignoring.");
                    continue;
                }


                match buf[2] {
                    0x01 => {
                        match insert_card(InsertCardAction { player_number: buf[3], card_id: [buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]] }) {
                            Ok(_card_id) => {
                                trigger_card_read(&device);
                            },
                            Err(e) => {
                                eprintln!("IO Error: {}", e);
                                thread::sleep(time::Duration::from_millis(1000));
                            }
                        };
                    },
                    0x02 => {
                        match remove_card(RemoveCardAction { player_number: buf[3] }) {
                            Ok(_card_id) => {},
                            Err(e) => {
                                eprintln!("IO Error: {}", e);
                                thread::sleep(time::Duration::from_millis(1000));
                            }
                        };
                    },
                    _ => { println!("Unknown action, ignoring."); },
                }
            },
            Err(e) => {
                eprintln!("Error: {}", e);
                return;
            }
        }
    }
}

fn insert_card(action: InsertCardAction) -> Result<String, io::Error> {
    println!("Card inserted into reader #{}: {:02X?}", action.player_number, action.card_id);
    // Create/Open file
    let mut file = File::create("card.txt")?;

    // Build card id string
    let mut card_id = String::new();
    for i in 0..action.card_id.len() {
        card_id.push_str(&format!("{:02X}", action.card_id[i]));
    }

    // Write to file
    match file.write(card_id.as_bytes()) {
        Ok(_result) => {},
        Err(e) => {
            eprintln!("Error writing card.txt: {}", e);
            return Err(e);
        }
    }

    return Ok(card_id);
}

fn remove_card(action: RemoveCardAction) -> Result<String, io::Error> {
    println!("Card removed from reader #{}", action.player_number);
    return Ok(String::new());
}

fn trigger_card_read(device: &hidapi::HidDevice) {
    // Write to device (Tell it to push the "Card Insert" keyboard key)
    match device.write(&[0; 8]) {
        Ok(_result) => {},
        Err(e) => {
            eprintln!("Error sending Card Insert signal to device: {}", e);
        }
    }
}
