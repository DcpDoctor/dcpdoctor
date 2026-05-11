use serde::{Deserialize, Serialize};
use std::path::Path;

/// Difference between two DCPs.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct DiffResult {
    pub differences: Vec<DiffEntry>,
    pub identical: bool,
}

/// A single difference entry.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DiffEntry {
    pub category: String,
    pub description: String,
    pub value_a: String,
    pub value_b: String,
}

/// Compare two DCP directories.
pub fn diff_dcps(dcp_a: &Path, dcp_b: &Path, compare_hashes: bool) -> DiffResult {
    let mut result = DiffResult::default();

    let dcp_a = match crate::dcp::open_dcp(dcp_a) {
        Ok(d) => d,
        Err(_) => {
            result.differences.push(DiffEntry {
                category: "structure".to_string(),
                description: "Failed to open DCP A".to_string(),
                value_a: String::new(),
                value_b: String::new(),
            });
            return result;
        }
    };

    let dcp_b = match crate::dcp::open_dcp(dcp_b) {
        Ok(d) => d,
        Err(_) => {
            result.differences.push(DiffEntry {
                category: "structure".to_string(),
                description: "Failed to open DCP B".to_string(),
                value_a: String::new(),
                value_b: String::new(),
            });
            return result;
        }
    };

    // Compare standards
    if dcp_a.standard != dcp_b.standard {
        result.differences.push(DiffEntry {
            category: "standard".to_string(),
            description: "Different DCP standard".to_string(),
            value_a: format!("{}", dcp_a.standard),
            value_b: format!("{}", dcp_b.standard),
        });
    }

    // Compare asset counts
    if dcp_a.assetmap.assets.len() != dcp_b.assetmap.assets.len() {
        result.differences.push(DiffEntry {
            category: "structure".to_string(),
            description: "Different number of assets".to_string(),
            value_a: format!("{}", dcp_a.assetmap.assets.len()),
            value_b: format!("{}", dcp_b.assetmap.assets.len()),
        });
    }

    // Compare CPL count
    if dcp_a.cpls.len() != dcp_b.cpls.len() {
        result.differences.push(DiffEntry {
            category: "structure".to_string(),
            description: "Different number of CPLs".to_string(),
            value_a: format!("{}", dcp_a.cpls.len()),
            value_b: format!("{}", dcp_b.cpls.len()),
        });
    }

    // Compare CPL titles
    for (i, ((_, cpl_a), (_, cpl_b))) in dcp_a.cpls.iter().zip(dcp_b.cpls.iter()).enumerate() {
        if cpl_a.content_title != cpl_b.content_title {
            result.differences.push(DiffEntry {
                category: "content".to_string(),
                description: format!("CPL {} title differs", i + 1),
                value_a: cpl_a.content_title.clone(),
                value_b: cpl_b.content_title.clone(),
            });
        }
    }

    let _ = compare_hashes; // TODO: implement hash comparison

    result.identical = result.differences.is_empty();
    result
}
