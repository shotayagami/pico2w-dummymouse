use std::{env, fs, path::PathBuf};

fn main() {
    let interval = env::var("DUMMYMOUSE_REPORT_INTERVAL_MS").unwrap_or_else(|_| "60000".into());
    let interval: u32 = interval
        .parse()
        .expect("DUMMYMOUSE_REPORT_INTERVAL_MS must be an integer");
    let output = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR is set")).join("interval.rs");
    fs::write(
        output,
        format!("pub const REPORT_INTERVAL_MS: u32 = {interval};\n"),
    )
    .expect("write generated interval constant");
    println!("cargo:rerun-if-env-changed=DUMMYMOUSE_REPORT_INTERVAL_MS");
}
