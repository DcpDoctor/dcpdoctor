use serde::{Deserialize, Serialize};
use std::path::Path;

/// DCP information summary.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct DcpInfo {
    pub standard: String,
    pub title: String,
    pub content_kind: String,
    pub asset_count: usize,
    pub cpl_count: usize,
    pub pkl_count: usize,
    pub reel_count: usize,
    pub total_duration_frames: i64,
}

/// Get summary information about a DCP.
pub fn get_dcp_info(dcp_dir: &Path) -> Option<DcpInfo> {
    let dcp = crate::dcp::open_dcp(dcp_dir).ok()?;

    let mut info = DcpInfo {
        standard: format!("{}", dcp.standard),
        asset_count: dcp.assetmap.assets.len(),
        cpl_count: dcp.cpls.len(),
        pkl_count: dcp.pkls.len(),
        ..Default::default()
    };

    if let Some((_, cpl)) = dcp.cpls.first() {
        info.title = cpl.content_title.clone();
        info.content_kind = cpl.content_kind.clone();
        info.reel_count = cpl.reels.len();
        info.total_duration_frames = cpl.reels.iter().map(|r| r.picture.duration).sum();
    }

    Some(info)
}
