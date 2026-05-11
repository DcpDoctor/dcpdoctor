use base64::Engine;
use base64::engine::general_purpose::STANDARD as BASE64;
use sha1::Digest;
use std::io::Read;
use std::path::Path;

const BUF_SIZE: usize = 65536;

/// Compute SHA-1 hash of a file, returning base64-encoded digest.
pub fn sha1_base64(path: &Path) -> std::io::Result<String> {
    let mut file = std::fs::File::open(path)?;
    let mut hasher = sha1::Sha1::new();
    let mut buf = [0u8; BUF_SIZE];

    loop {
        let n = file.read(&mut buf)?;
        if n == 0 {
            break;
        }
        hasher.update(&buf[..n]);
    }

    let digest = hasher.finalize();
    Ok(BASE64.encode(digest))
}

/// Compute SHA-1 hash of a file, returning hex-encoded digest.
pub fn sha1_hex(path: &Path) -> std::io::Result<String> {
    let mut file = std::fs::File::open(path)?;
    let mut hasher = sha1::Sha1::new();
    let mut buf = [0u8; BUF_SIZE];

    loop {
        let n = file.read(&mut buf)?;
        if n == 0 {
            break;
        }
        hasher.update(&buf[..n]);
    }

    let digest = hasher.finalize();
    Ok(digest.iter().map(|b| format!("{b:02x}")).collect())
}
