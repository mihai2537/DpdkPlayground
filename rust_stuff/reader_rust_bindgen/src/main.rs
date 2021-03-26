#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#[allow(dead_code)]
mod bindings;

use bindings::rte_eal_init;
use std::ffi::CString;

fn main() {
    println!("Hello, world!");

    let mut args = vec![
        CString::new("./executabil")
            .expect("Nu a mers.\n")
            .into_raw(),
        CString::new("-l").expect("Nu a mers.\n").into_raw(),
        CString::new("1").expect("Nu a mers.\n").into_raw(),
        CString::new("--proc-type=secondary")
            .expect("Nu a mers.\n")
            .into_raw(),
    ];

    let args = args.as_mut_ptr();

    let cnt = 4;

    let ret_val = unsafe { rte_eal_init(cnt as i32, args) };

    // let args = unsafe { Vec::from_raw_parts(args, cnt, cnt) };

    // for val in args {
    //     unsafe { CString::from_raw(val) };
    // }

    if 0 > ret_val {
        println!("Eroare, nu a mers rte_eal_init.");
    } else {
        println!("Este BAAAA A MERS NENOROCIREA MANCA-V-AS");
    }
}
