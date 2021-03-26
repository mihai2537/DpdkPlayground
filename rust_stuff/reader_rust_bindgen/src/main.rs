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

    // let mut args = vec![
    //     "./executabil\0".as_mut_ptr(),
    //     "-l\0".as_mut_ptr(),
    //     "1\0".as_mut_ptr(),
    //     "--proc-type=secondary\0".as_mut_ptr(),
    // ];

    let args = args.as_mut_ptr();

    // let args: Vec<String> = env::args().collect();
    let cnt = 4;

    let ret_val = unsafe { rte_eal_init(cnt as i32, args) };

    if 0 > ret_val {
        println!("Eroare, nu a mers rte_eal_init.");
    } else {
        println!("Este BAAAA A MERS NENOROCIREA MANCA-V-AS");
    }
}
