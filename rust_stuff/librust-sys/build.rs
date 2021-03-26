fn main() {
    println!("cargo:rustc-link=rte_eal");
    println!("rustc-link-search=/usr/local/lib/x86_64-linux-gnu");
}
