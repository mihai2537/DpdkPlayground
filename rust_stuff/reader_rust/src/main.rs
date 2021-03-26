use std::os::raw::c_char;

#[link(name = "rte_eal")]
extern "C" {
    fn rte_eal_init(argc: i32, argv: *mut *const u8) -> i32;
    fn rte_eal_process_type() -> i32;
    fn rte_exit(exit_code: i32, format: *const c_char);
}

fn main() {
    println!("Hello, world!");

    let mut args = vec![
        "./reader_rust\0".as_ptr(),
        "-l\0".as_ptr(),
        "1\0".as_ptr(),
        "--proc-type=secondary\0".as_ptr(),
    ];

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

// Minimal implementation of single precision complex numbers
#[repr(C)]
#[derive(Clone, Copy)]
enum rte_proc_type_t {
    RteProcAuto = -1,   /* allow auto-detection of primary/secondary */
    RteProcPrimary = 0, /* set to zero, so primary is the default */
    RteProcSecondary,
    RteProcInvalid,
}
