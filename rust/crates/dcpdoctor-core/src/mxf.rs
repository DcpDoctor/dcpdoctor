use serde::{Deserialize, Serialize};
use std::path::Path;

/// Picture descriptor from MXF.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PictureDescriptor {
    pub width: u32,
    pub height: u32,
    pub frame_rate_num: u32,
    pub frame_rate_den: u32,
    pub bit_depth: u32,
    pub frame_count: u64,
}

/// Sound descriptor from MXF.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SoundDescriptor {
    pub sample_rate: u32,
    pub channels: u32,
    pub bit_depth: u32,
    pub duration: u64,
}

/// MXF file information.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct MxfInfo {
    pub valid: bool,
    pub error: String,
    pub essence_type: String,
    pub picture: Option<PictureDescriptor>,
    pub sound: Option<SoundDescriptor>,
}

/// Read MXF file metadata.
///
/// This is a basic implementation that reads KLV header metadata.
/// Full MXF parsing requires asdcplib FFI bindings.
pub fn read_mxf_info(path: &Path) -> MxfInfo {
    // Read the first bytes to check for MXF header
    let data = match std::fs::read(path) {
        Ok(d) => d,
        Err(e) => {
            return MxfInfo {
                valid: false,
                error: format!("Failed to read file: {e}"),
                ..Default::default()
            };
        }
    };

    // MXF files start with a partition pack key (06 0e 2b 34)
    if data.len() < 16 || data[0..4] != [0x06, 0x0e, 0x2b, 0x34] {
        return MxfInfo {
            valid: false,
            error: "Not a valid MXF file (missing SMPTE UL header)".to_string(),
            ..Default::default()
        };
    }

    // Basic validation passed — full parsing requires asdcplib
    MxfInfo {
        valid: true,
        error: String::new(),
        essence_type: "unknown".to_string(),
        picture: None,
        sound: None,
    }
}
