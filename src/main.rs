use std::thread;
use std::time;
use std::fs::File;
use std::io::prelude::*;
use hidapi::HidApi;

struct InsertCardAction {
    player_number: u8,
    card_id: [u8; 8],
}

struct RemoveCardAction {
    player_number: u8,
}


fn main() {
    extern crate hidapi;

    let api = HidApi::new().unwrap();

    for device in api.devices() {
        println!("{:#?}", device);
    }

    loop {
        // Connect to device using its VID and PID
        println!("Attempting to connect to device...");
        let (vid, pid) = (5824, 1158);
        match api.open(vid, pid) {
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
                device_poll(device);
            },
            Err(e) => {
                eprintln!("Error: {}", e);
                thread::sleep(time::Duration::from_millis(1000));
            }
        };
    }
}

fn device_poll(device: hidapi::HidDevice) {
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
                    0x01 => insert_card(InsertCardAction { player_number: buf[3], card_id: [buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]] }, &device),
                    0x02 => remove_card(RemoveCardAction { player_number: buf[3] }),
                       _ => println!("Unknown action, ignoring.")
                }
            },
            Err(e) => {
                eprintln!("Error: {}", e);
                return;
            }
        }
    }
}

fn insert_card(action: InsertCardAction, device: &hidapi::HidDevice) {
    println!("Card inserted into reader #{}: {:02X?}", action.player_number, action.card_id);
    // Write to file
    let mut file = File::create("card.txt").unwrap();
    for i in 0..action.card_id.len() {
        file.write_fmt(format_args!("{:02X}", action.card_id[i]));
    }
    device.write(&[0; 8]);
}

fn remove_card(action: RemoveCardAction) {
    println!("Card removed from reader #{}", action.player_number);
}
