#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#[allow(dead_code)]
mod bindingsMbuf;

use bindingsMbuf::{
    rte_eal_init, rte_eal_process_type, rte_mbuf, rte_mempool_lookup,
    rte_proc_type_t_RTE_PROC_PRIMARY, rte_ring_dequeue_real, rte_ring_lookup,
};

use std::ffi::CString;
use std::os::raw::c_void;
use std::ptr::null_mut;

use std::process;
use std::{thread, time};

fn main() {
    println!("Hello, world!");

    let m1 = CString::new("./executabil").expect("Nu a mers.\n");
    let m2 = CString::new("-l").expect("Nu a mers.\n");
    let m3 = CString::new("1").expect("Nu a mers.\n");
    let m4 = CString::new("--proc-type=secondary").expect("Nu a mers.\n");

    let mut args = vec![m1.as_ptr(), m2.as_ptr(), m3.as_ptr(), m4.as_ptr()];

    let my_args = args.as_mut_ptr();

    let cnt: i32 = 4;

    let ret_val = unsafe { rte_eal_init(cnt, my_args) };

    if 0 > ret_val {
        println!("Eroare, nu a mers rte_eal_init.");
    } else {
        println!("Este BAAAA A MERS NENOROCIREA MANCA-V-AS");
    }

    if unsafe { rte_eal_process_type() } == rte_proc_type_t_RTE_PROC_PRIMARY {
        println!("Error, --proc-type=secondary shall be used.\n");
        process::exit(1);
    }

    let receive_ring_name = CString::new("PRI_2_SEC").expect("Nu a mers.\n");

    let recv_ring = unsafe { rte_ring_lookup(receive_ring_name.as_ptr()) };
    if recv_ring.is_null() {
        println!("I could not get the rte_ring for recv. ERROR.\n");
    } else {
        println!("SUCCESS, Got the receive buffer.");
    }

    let send_ring_name = CString::new("SEC_2_PRI").expect("Nu a mers.\n");

    let send_ring = unsafe { rte_ring_lookup(send_ring_name.as_ptr()) };
    if send_ring.is_null() {
        println!("I could not get the rte_ring for send.\n");
    } else {
        println!("SUCCESS, Got the send buffer.");
    }

    let mempool_name = CString::new("MSG_POOL").expect("Nu a mers.\n");

    let mempool = unsafe { rte_mempool_lookup(mempool_name.as_ptr()) };
    if mempool.is_null() {
        println!("I could not get the rte_mempool!\n");
    } else {
        println!("SUCCES, Got the mempool");
    }

    let mut packets_received: i32;
    let sleep_time = time::Duration::from_secs(5);

    let mut result: *mut c_void = null_mut();

    loop {
        packets_received = unsafe { rte_ring_dequeue_real(recv_ring, &mut result) };
        if 0 > packets_received {
            println!("Nothing on the Queue, so I need to sleep a little.");
            thread::sleep(sleep_time);
        } else {
            println!("Hooray, Packet Received in RUST");
        }
    }
}
// This was memory leak prone because you had to call from_raw(ptr) to get those things back.

// let mut args = vec![
//     CString::new("./executabil")
//         .expect("Nu a mers.\n")
//         .into_raw(),
//     CString::new("-l").expect("Nu a mers.\n").into_raw(),
//     CString::new("1").expect("Nu a mers.\n").into_raw(),
//     CString::new("--proc-type=secondary")
//         .expect("Nu a mers.\n")
//         .into_raw(),
// ];

// let args = unsafe { Vec::from_raw_parts(args, cnt, cnt) };

// for val in args {
//     unsafe { CString::from_raw(val) };
// }
